#include"Game.h"

Array<Array<Array<Array<int>>>> comb_calc() {
	Array<Array<Array<Array<int>>>> res(9, Array<Array<Array<int>>>(9));
	int M = 256; // = 2^8
	for (int bit = 1; bit < M; bit++) {
		int msb = 0;
		Array<int> line;
		int r = 0;
		for (int len = 0; len < 8; len++) {
			if (bit & (1 << len)) {
				msb = len;
				r++;
				line.push_back(len);
			}
		}
		for (int i = msb; i < 8; i++) {
			res[i][r].push_back(line);
		}
	}
	return res;
}

AI::AI(int32 w, int32 h, int32 m) {
	reset(w, h, m);
	comb_master = comb_calc();
}

//���s�\���ǂ�����Ԃ�
void AI::run(Field f) {
	// �Ֆʂ�1�x���G���Ă��Ȃ��ꍇ�͓K���ȏꏊ���J����
	if (!in_game) {
		reset(W, H, M);
		in_game = true;
		Print << U"ai_start";
		command_open.push(Vec2(Random(W - 1), Random(H - 1)));
		return;
	}

	// �񓯊��ōl��������
	if (!thinking) {
		// �l���ĂȂ���ԂȂ�l��������
		// ���ݎ󂯎���Ă�Ֆʂ����ōl����
		get_update(f);
		task = std::async(std::launch::async, [&] {return think(); });
		thinking = true;
	}

}

Vec2 AI::ai_open() {
	Vec2 res = command_open.front();
	command_open.pop();
	return res;
}

Vec2 AI::ai_build_flag() {
	Vec2 res = command_build_flag.front();
	command_build_flag.pop();
	return res;
}

void AI::get_update(Field f) {

	std::queue<Vec2> nx_unsolve;
	while (unsolved_pos.size() != 0) {
		//���������Ă���W�͍X�V����K�v���Ȃ�
		Vec2 pos = unsolved_pos.front();
		unsolved_pos.pop();

		Tile& t = field[pos.y][pos.x];

		// �������Ă���2�x�ƖK�₵�Ȃ�
		t.flag = f.get_on_flag(pos);
		if (t.flag) {
			t.need_think = false;
			found_mines++;
			// �ǂݔ�΂��܂�
			continue;
		}

		// �J���Ă��琔�������炢�܂�
		t.open = f.get_on_open(pos);
		if (t.open)t.num = f.get_on_num(pos);

		t.probability_flag = 0;

		// �X�V������ɍl����K�v��������W�͂ǂ���
		// ���Ȃ��}�X 
		if (!t.open) {
			// can't think yet.
			nx_unsolve.push(pos);
		}
		// num == 0 �� �l����K�v���Ȃ��}�X�Ȃ̂Ŗ���
		// ��}�X �� num != 0
		else if (t.num != 0) {
			// �l����]�n����
			int flag_count = 0, closed_count = 0;
			for (Vec2 nx_p : look_around(pos)) {
				if (field[nx_p.y][nx_p.x].flag)flag_count++;
				else if (!field[nx_p.y][nx_p.x].open)closed_count++;
			}
			if (closed_count == 0 && flag_count == t.num) {
				// �����l���Ȃ��Ă���
				continue;
			}
			// ����ȊO�͂܂��l����]�n����
			else need_think_pos.push(pos);
		}
	}
	unsolved_pos = nx_unsolve;

}

void AI::reset(int32 w, int32 h, int32 m) {
	W = w;
	H = h;
	M = m;
	//��������
	while (unsolved_pos.size() != 0)unsolved_pos.pop();
	while (command_open.size() != 0)command_open.pop();
	while (command_build_flag.size() != 0)command_build_flag.pop();
	// ����͑S��unsolved_pos
	for (int i = 0; i < H; i++) {
		Array<Tile> line;
		for (int j = 0; j < W; j++) {
			Tile  t;
			line.push_back(t);
			unsolved_pos.push(Vec2(j, i));
		}
		field.push_back(line);
	}
	sum_mines = H * W * (M / 100.0);
	found_mines = 0;
	in_game = false;
	thinking = false;
	stalemate = false;
}

bool AI::think() {
	// �Ֆʂ��J���ĂȂ��ꍇ��
	if (suggest_build_flag()) { death_count = 0;  return false; }
	else if (suggest_open()) { death_count = 0; return false; }

	if (death_count == 2) {
		// �����Ř_��(�����d��)�����s���Ă݂�
		if (logic2()) {
			Print << U"LOGIC";
			death_count = 0;
			return false;
		}

		// ����ł������Ȃ痐��������
		random();
		Print << need_think_pos.size();
		death_count = 0;
	}

	return true;
}

bool AI::logic() {
	bool res = false;

	std::queue<Vec2> nx_need;
	while (need_think_pos.size() != 0) {
		Vec2 pos = need_think_pos.front();
		need_think_pos.pop();

		// �l���Ă���͂̕}�X���
		Array<Vec2>  close_pos;
		int flag_count = 0;
		for (Vec2 nx_p : look_around(pos)) {
			Tile t = field[nx_p.y][nx_p.x];
			if (!t.flag && !t.open)close_pos.push_back(nx_p);
			else if (t.flag)flag_count++;
		}

		// �}�X�̏󋵕ω��ŉe���̂�����W��񋓂��Ă���
		Array<Vec2> confirm_pos;
		for (Vec2 cl_pos : close_pos) {
			// �}�X����8�}�X
			for (Vec2 nx_pos : look_around(cl_pos)) {
				Tile t = field[nx_pos.y][nx_pos.x];
				// �J���Ă��邱�Ƃƌ����W�łȂ��ꍇ���؍��W�ɂȂ�
				if (t.open && t.num != 0 && nx_pos != pos && confirm_pos.count(nx_pos) == 0) {
					confirm_pos.push_back(nx_pos);
				}
			}
		}

		// �����܂�ok

		int pattern = 0;

		// ���͂̕}�X C num �̒ʂ��
		int N = close_pos.size();
		int R = field[pos.y][pos.x].num - flag_count;
		for (Array<int> line : comb_master[N - 1][R]) {
			// ���z�t�B�[���h����
			Array<Array<Tile>> vf = field;
			// �Ƃ�ܓK���ɒu��
			for (int v : line) {
				Tile& t = vf[close_pos[v].y][close_pos[v].x];
				t.flag = true;
			}
			// �u�����Ƃ��ȊO�͊J�����Ⴄ
			for (Vec2 nx_pos : look_around(pos)) {
				// �t���O���Ȃ��Ƃ���S���J���Ă݂�
				if (!vf[nx_pos.y][nx_pos.x].flag)vf[nx_pos.y][nx_pos.x].open = true;
			}

			bool ok = true;
			// ���̏�Ԃ����蓾�邩 confirm_pos �Ō��؂���
			for (Vec2 c_pos : confirm_pos) {
				// sum_flag + close >= num �Ȃ�ok
				int sum_flag = 0, sum_close = 0;
				for (Vec2 nx_pos : look_around(c_pos)) {
					Tile t = vf[nx_pos.y][nx_pos.x];
					if (t.flag)sum_flag++;
					else if (!t.flag && !t.open)sum_close++;
				}
				// ���藧���Ȃ��ꍇ�̓_��
				if ((sum_flag + sum_close) < vf[c_pos.y][c_pos.x].num) {
					ok = false;
					break;
				}
			}

			// �S��ok�ȏꍇ
			if (ok) {
				// ���̒u�����͂��蓾��
				pattern++;
				// ���݊m��������U��
				for (int v : line) {
					Tile& t = field[close_pos[v].y][close_pos[v].x];
					t.probability_flag++;
				}
			}

		}

		// �S�p�^�[���񋓂����
		// ���݊m���̊m�F
		for (Vec2 cl_pos : close_pos) {
			// �S�ʂ�ł����ɂȂ��Ƃ��蓾�Ȃ��ꍇ�m��
			if (pattern == field[cl_pos.y][cl_pos.x].probability_flag) {
				command_build_flag.push(cl_pos);
				res = true;
			}
			// �����Ɋ�������Ɛ�΂ɂȂ肽���Ȃ��ꍇ
			else if (0 == field[cl_pos.y][cl_pos.x].probability_flag) {
				command_open.push(cl_pos);
				res = true;
			}
		}

		// �m�F�I������瑶�݊m���߂�
		for (Vec2 nx_pos : look_around(pos)) {
			field[nx_pos.y][nx_pos.x].probability_flag = 0;
		}

		nx_need.push(pos);
	}

	need_think_pos = nx_need;


	return res;
}

// s �� mask �ŉ��H���܂�
std::set<int32> difference(std::set<int32> mask, std::set<int32> s) {
	std::set<int32> res = s;
	for (auto v : mask) {
		res.erase(v);
	}
	return res;
}

// s0 �� s1 �Ɋ��S�����ꍇ
bool inclus(std::set<int32> s0, std::set<int32> s1) {
	bool res = true;
	for (auto v : s0) {
		if (!(s1.contains(v)))res = false;
	}
	return res;
}

// s0 s1 �̋��ʗ��ς����܂�
std::set<int32> common(std::set<int32> s0, std::set<int32> s1) {
	std::set<int32> res;
	for (auto v : s0) {
		if (s1.contains(v))res.insert(v);
	}
	return res;
}

bool AI::logic2() {
	// �������1�ł��񎦂ł����True
	bool res = false;

	std::queue<Vec2> nx_need;
	// �}�X�S�񋓊����A�����ŉ����瓖�Ă͂߂Ă���
	Array<std::pair<std::set<int32>, int32>> dictionary;
	while (need_think_pos.size() != 0) {
		Vec2 pos = need_think_pos.front();
		need_think_pos.pop();
		// pos���ӂ̕}�X�ɑ΂��ĕ��z����set��dic�ɓ����
		int32 sum_flag = 0;
		std::set<int32> line;
		for (Vec2 nx_pos : look_around(pos)) {
			if (field[nx_pos.y][nx_pos.x].flag)sum_flag++;
			else if (!field[nx_pos.y][nx_pos.x].flag && !field[nx_pos.y][nx_pos.x].open) {
				int32 yx = nx_pos.y * H + nx_pos.x;
				line.insert(yx);
			}
		}

		dictionary.push_back(std::make_pair(line, field[pos.y][pos.x].num - sum_flag));
		nx_need.push(pos);
	}
	need_think_pos = nx_need;

	// ���������邾�������Ă���
	while (true) {
		bool now_update = true;

		// 2�d���[�v�łȂ񂩒T��
		int M = dictionary.size();
		for (int i = 0; i < M; i++) {
			for (int j = i + 1; j < M; j++) {
				auto piece_0 = dictionary[i];
				auto piece_1 = dictionary[j];
				std::set<int32> elem_0 = piece_0.first;
				std::set<int32> elem_1 = piece_1.first;
				int ans_0 = piece_0.second;
				int ans_1 = piece_1.second;
				// 0 ��v�f���Ȃ��ق���
				if (elem_0.size() > elem_1.size()) {
					std::swap(elem_0, elem_1);
				}

				// �����ꍇ
				if (inclus(elem_0, elem_1)) {
					// �P�������Z�̂ݍs��
					elem_1 = difference(elem_0, elem_1);
					ans_1 -= ans_0;

					// ����{ A,B,C,... } = 0�̂Ƃ������͑S��0
					if (ans_1 == 0) {
						// open �R�}���h�ɓ���邱�Ƃ��o���Ă��̎��͔j���ł���
						for (int32 yx : elem_1) {
							Vec2 p(yx % H, yx / H);
							command_open.push(p);
							res = true;
						}
					}
					// �P��v�f�̂ݎc��ꍇ
					else if (elem_1.size() == 1) {
						int yx = *elem_1.begin();
						Vec2 p(yx % H, yx / H);
						// second == 1 �Ȃ� flag
						if (ans_1 == 1) {
							command_build_flag.push(p);
							res = true;
						}
						// second == 0 �Ȃ� open
						else if (ans_1 == 0) {
							command_open.push(p);
							res = true;
						}
					}
					continue;
				}
				// ����Ȃ��ꍇ
				else {
					continue;
					// ���ʕ�����������
					std::set<int32> friend_piece = common(elem_0, elem_1);
					// ���ꂼ�ꂩ�狤�ʕ������킮
					auto conv_0 = difference(friend_piece, elem_0);
					auto conv_1 = difference(friend_piece, elem_1);

					// fp���������鐔
					int32 M = std::min((int32)(friend_piece.size()), std::min(ans_0, ans_1));
					// �S�ʂ莎���Ă݂�
					int ok_ans = -1;
					int unique = 0;
					for (int fp_ans = 0; fp_ans <= M; fp_ans++) {
						if (fp_ans <= ans_0 && ans_0 <= (fp_ans + conv_0.size())
							&& fp_ans <= ans_1 && ans_1 <= (fp_ans + conv_1.size())) {
							unique++;
							ok_ans = fp_ans;
						}
					}
					// ���ɒP��ȊO�̏󋵂Ő��藧�ꍇ�m��Ƃ͌����Ȃ�
					if (unique != 1)continue;

					// ���̏ꍇ��������
					int cv_ans_0 = ans_0 - ok_ans;
					int cv_ans_1 = ans_1 - ok_ans;

					// cv_anc == 0 �̂Ƃ��A�v�f�Q�͑S��0�ɂł���
					if (cv_ans_0 == 0) {
						// conv_0 �͑S����@�w��
						for (int32 yx : conv_0) {
							Vec2 p(yx % H, yx / H);
							command_open.push(p);
							res = true;
						}
					}
					// ������0�Ȃ�S�����
					else if (cv_ans_1 == 0) {
						for (int32 yx : conv_1) {
							Vec2 p(yx % H, yx / H);
							command_open.push(p);
							res = true;
						}
					}
					// fp_ans == 0 �Ȃ� friend_piece �͑S�J��
					else if (ok_ans == 0) {
						for (int32 yx : friend_piece) {
							Vec2 p(yx % H, yx / H);
							command_open.push(p);
							res = true;
						}
					}
					// cv_ans_0 == conv_0.size() �Ȃ�S��1
					else if (cv_ans_0 == conv_0.size()) {
						for (int32 yx : conv_0) {
							Vec2 p(yx % H, yx / H);
							command_build_flag.push(p);
							res = true;
						}
					}
					// cv_ans_1 == conv_1.size() �Ȃ�S��1
					else if (cv_ans_1 == conv_1.size()) {
						for (int32 yx : conv_1) {
							Vec2 p(yx % H, yx / H);
							command_build_flag.push(p);
							res = true;
						}
					}
					// fp_ans == friend_piece �Ȃ�S��1
					else if (ok_ans == friend_piece.size()) {
						for (int32 yx : friend_piece) {
							Vec2 p(yx % H, yx / H);
							command_build_flag.push(p);
							res = true;
						}
					}


				}
			}
		}

		if (now_update)break;
	}

	return res;
}

void AI::random() {
	Print << U"random";
	int32 select = Random(unsolved_pos.size());
	int32 count = 0;
	std::queue<Vec2> nx_suggest;
	while (unsolved_pos.size() != 0) {
		if (count == select) { command_open.push(unsolved_pos.front()); need_think_pos.push(unsolved_pos.front()); }
		else nx_suggest.push(unsolved_pos.front());
		unsolved_pos.pop();
		count++;
	}
	unsolved_pos = nx_suggest;
}

bool AI::suggest_build_flag() {
	bool res = false;
	std::queue<Vec2> nx_need;
	while (need_think_pos.size() != 0) {
		// �������̍��W�ɂ��čl���܂�
		Vec2 pos = need_think_pos.front();
		need_think_pos.pop();
		Tile& t = field[pos.y][pos.x];

		// �J���ĂĐ������킩��ꍇ
		Array<Vec2> nx_pos = look_around(pos);

		// ���Ƀt���O�������Ă��鐔�𐔂���
		int32 sum_flag = 0;
		// ���ɋ󂢂Ă���}�X�𐔂���
		int32 sum_open = 0;

		int32 sum_closed = 0;


		for (Vec2 p : nx_pos) {
			Tile nx_t = field[p.y][p.x];
			if (nx_t.flag)sum_flag++;
			if (nx_t.open)sum_open++;
			if (!nx_t.open)sum_closed++;
		}


		// �}�X == ���� �Ȃ�S�����ł����A���łɗ����Ă�Ȃ�w�߂Ȃ�
		if (sum_closed == t.num) {
			for (Vec2 p : nx_pos) {
				Tile nx_t = field[p.y][p.x];
				// �J���ĂȂ� & flag �����ĂȂ��ꍇ
				if (!nx_t.open && !nx_t.flag) {
					nx_t.flag = true;
					command_build_flag.push(p);
					res = true;
				}
			}
		}
		// �܂�������Ȃ��ꍇ��need_think�ɖ߂��܂�
		else {
			nx_need.push(pos);
		}
	}
	need_think_pos = nx_need;
	return res;
}

bool AI::suggest_open() {
	bool res = false;
	std::queue<Vec2> nx_need;
	while (need_think_pos.size() != 0) {
		// �������̍��W�ɂ��čl���܂�
		Vec2 pos = need_think_pos.front();
		need_think_pos.pop();
		Tile& t = field[pos.y][pos.x];

		// �}�X�̐�����������Ȃ��Ȃ�����ł��Ȃ�
		if (!t.open) {
			continue;
		}

		// �J���ĂĐ������킩��ꍇ
		Array<Vec2> nx_pos = look_around(pos);

		// ���Ƀt���O�������Ă��鐔�𐔂���
		int32 sum_flag = 0;
		// ���ɋ󂢂Ă���}�X�𐔂���
		int32 sum_open = 0;

		int32 sum_closed = 0;


		for (Vec2 p : nx_pos) {
			Tile nx_t = field[p.y][p.x];
			if (nx_t.flag)sum_flag++;
			if (nx_t.open)sum_open++;
			if (!nx_t.open)sum_closed++;
		}


		// num == sum_flag �̂Ƃ��Aflag�Ȃ��܂��Ă�}�X�S���J��
		// num == sum_closed + sum_flag ��flag�Ȃ��}�X�S���J����
		if (sum_flag == t.num) {
			for (Vec2 p : nx_pos) {
				Tile nx_t = field[p.y][p.x];
				// �J���ĂȂ� & flag �����ĂȂ��ꍇ
				if (!nx_t.open && !nx_t.flag) {
					nx_t.open = true;
					command_open.push(p);
					res = true;
				}
			}
		}
		//����Ȃ��Ȃ�čl
		else {
			nx_need.push(pos);
			continue;
		}
	}
	need_think_pos = nx_need;
	return false;
}

Array<Vec2> AI::look_around(Vec2 pos) {
	Array<Vec2> res;
	for (int dy = -1; dy < 2; dy++) {
		for (int dx = -1; dx < 2; dx++) {
			if (pos.y + dy < 0 || pos.y + dy >= H || pos.x + dx < 0 || pos.x + dx >= W || (dx == 0 && dy == 0))continue;
			res.push_back(Vec2(pos.x + dx, pos.y + dy));
		}
	}
	return res;
}

bool AI::is_thinking() {
	// �����������s���ĂȂ��ꍇ�͎��s����΂������
	if (!thinking)return false;

	// ���s���ł���ΏI��������ǂ������m�F������
	std::future_status status;
	status = task.wait_for(0ms);
	if (status == std::future_status::ready) {
		// �����������Ă�
		stalemate = task.get();
		// ��邱�ƂȂ��Ȃ玀�̃J�E���g�𑝂₷
		if (stalemate)death_count++;

		thinking = false;
	}
	else return false;
}

bool AI::is_stalemate() {
	return (death_count == 2);
}

bool AI::exist_command_open() {
	return command_open.size() != 0;
}

bool AI::exist_command_build_flag() {
	return command_build_flag.size() != 0;
}

