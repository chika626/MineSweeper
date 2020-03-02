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

//続行可能かどうかを返す
void AI::run(Field f) {
	// 盤面が1度も触られていない場合は適当な場所を開ける
	if (!in_game) {
		reset(W, H, M);
		in_game = true;
		Print << U"ai_start";
		command_open.push(Vec2(Random(W - 1), Random(H - 1)));
		return;
	}

	// 非同期で考えさせる
	if (!thinking) {
		// 考えてない状態なら考えさせる
		// 現在受け取ってる盤面だけで考える
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
		//もう解けてる座標は更新する必要がない
		Vec2 pos = unsolved_pos.front();
		unsolved_pos.pop();

		Tile& t = field[pos.y][pos.x];

		// 旗立ってたら2度と訪問しない
		t.flag = f.get_on_flag(pos);
		if (t.flag) {
			t.need_think = false;
			found_mines++;
			// 読み飛ばします
			continue;
		}

		// 開いてたら数字をもらいます
		t.open = f.get_on_open(pos);
		if (t.open)t.num = f.get_on_num(pos);

		t.probability_flag = 0;

		// 更新した後に考える必要がある座標はどこか
		// 旗なし閉マス 
		if (!t.open) {
			// can't think yet.
			nx_unsolve.push(pos);
		}
		// num == 0 は 考える必要がないマスなので無視
		// 空マス で num != 0
		else if (t.num != 0) {
			// 考える余地あり
			int flag_count = 0, closed_count = 0;
			for (Vec2 nx_p : look_around(pos)) {
				if (field[nx_p.y][nx_p.x].flag)flag_count++;
				else if (!field[nx_p.y][nx_p.x].open)closed_count++;
			}
			if (closed_count == 0 && flag_count == t.num) {
				// もう考えなくていい
				continue;
			}
			// それ以外はまだ考える余地あり
			else need_think_pos.push(pos);
		}
	}
	unsolved_pos = nx_unsolve;

}

void AI::reset(int32 w, int32 h, int32 m) {
	W = w;
	H = h;
	M = m;
	//初期生成
	while (unsolved_pos.size() != 0)unsolved_pos.pop();
	while (command_open.size() != 0)command_open.pop();
	while (command_build_flag.size() != 0)command_build_flag.pop();
	// 初手は全てunsolved_pos
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
	// 盤面が開いてない場合は
	if (suggest_build_flag()) { death_count = 0;  return false; }
	else if (suggest_open()) { death_count = 0; return false; }

	if (death_count == 2) {
		// ここで論理(処理重い)を実行してみる
		if (logic2()) {
			Print << U"LOGIC";
			death_count = 0;
			return false;
		}

		// それでも無理なら乱択をする
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

		// 考えてる周囲の閉マスを列挙
		Array<Vec2>  close_pos;
		int flag_count = 0;
		for (Vec2 nx_p : look_around(pos)) {
			Tile t = field[nx_p.y][nx_p.x];
			if (!t.flag && !t.open)close_pos.push_back(nx_p);
			else if (t.flag)flag_count++;
		}

		// 閉マスの状況変化で影響のある座標を列挙しておく
		Array<Vec2> confirm_pos;
		for (Vec2 cl_pos : close_pos) {
			// 閉マス周囲8マス
			for (Vec2 nx_pos : look_around(cl_pos)) {
				Tile t = field[nx_pos.y][nx_pos.x];
				// 開いていることと現座標でない場合検証座標になる
				if (t.open && t.num != 0 && nx_pos != pos && confirm_pos.count(nx_pos) == 0) {
					confirm_pos.push_back(nx_pos);
				}
			}
		}

		// ここまでok

		int pattern = 0;

		// 周囲の閉マス C num の通り列挙
		int N = close_pos.size();
		int R = field[pos.y][pos.x].num - flag_count;
		for (Array<int> line : comb_master[N - 1][R]) {
			// 仮想フィールド生成
			Array<Array<Tile>> vf = field;
			// とりま適当に置く
			for (int v : line) {
				Tile& t = vf[close_pos[v].y][close_pos[v].x];
				t.flag = true;
			}
			// 置いたとこ以外は開けちゃう
			for (Vec2 nx_pos : look_around(pos)) {
				// フラグがないところ全部開けてみる
				if (!vf[nx_pos.y][nx_pos.x].flag)vf[nx_pos.y][nx_pos.x].open = true;
			}

			bool ok = true;
			// この状態があり得るか confirm_pos で検証する
			for (Vec2 c_pos : confirm_pos) {
				// sum_flag + close >= num ならok
				int sum_flag = 0, sum_close = 0;
				for (Vec2 nx_pos : look_around(c_pos)) {
					Tile t = vf[nx_pos.y][nx_pos.x];
					if (t.flag)sum_flag++;
					else if (!t.flag && !t.open)sum_close++;
				}
				// 成り立たない場合はダメ
				if ((sum_flag + sum_close) < vf[c_pos.y][c_pos.x].num) {
					ok = false;
					break;
				}
			}

			// 全部okな場合
			if (ok) {
				// この置き方はあり得る
				pattern++;
				// 存在確率を割り振る
				for (int v : line) {
					Tile& t = field[close_pos[v].y][close_pos[v].x];
					t.probability_flag++;
				}
			}

		}

		// 全パターン列挙おわり
		// 存在確率の確認
		for (Vec2 cl_pos : close_pos) {
			// 全通りでそこにないとあり得ない場合確定
			if (pattern == field[cl_pos.y][cl_pos.x].probability_flag) {
				command_build_flag.push(cl_pos);
				res = true;
			}
			// ここに旗があると絶対になりたたない場合
			else if (0 == field[cl_pos.y][cl_pos.x].probability_flag) {
				command_open.push(cl_pos);
				res = true;
			}
		}

		// 確認終わったら存在確率戻す
		for (Vec2 nx_pos : look_around(pos)) {
			field[nx_pos.y][nx_pos.x].probability_flag = 0;
		}

		nx_need.push(pos);
	}

	need_think_pos = nx_need;


	return res;
}

// s を mask で加工します
std::set<int32> difference(std::set<int32> mask, std::set<int32> s) {
	std::set<int32> res = s;
	for (auto v : mask) {
		res.erase(v);
	}
	return res;
}

// s0 が s1 に完全内包する場合
bool inclus(std::set<int32> s0, std::set<int32> s1) {
	bool res = true;
	for (auto v : s0) {
		if (!(s1.contains(v)))res = false;
	}
	return res;
}

// s0 s1 の共通列を変えします
std::set<int32> common(std::set<int32> s0, std::set<int32> s1) {
	std::set<int32> res;
	for (auto v : s0) {
		if (s1.contains(v))res.insert(v);
	}
	return res;
}

bool AI::logic2() {
	// 解決策を1つでも提示できればTrue
	bool res = false;

	std::queue<Vec2> nx_need;
	// 閉マス全列挙完了、辞書で下から当てはめていく
	Array<std::pair<std::set<int32>, int32>> dictionary;
	while (need_think_pos.size() != 0) {
		Vec2 pos = need_think_pos.front();
		need_think_pos.pop();
		// pos周辺の閉マスに対して分配したsetをdicに入れる
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

	// 式を解けるだけ解いていく
	while (true) {
		bool now_update = true;

		// 2重ループでなんか探す
		int M = dictionary.size();
		for (int i = 0; i < M; i++) {
			for (int j = i + 1; j < M; j++) {
				auto piece_0 = dictionary[i];
				auto piece_1 = dictionary[j];
				std::set<int32> elem_0 = piece_0.first;
				std::set<int32> elem_1 = piece_1.first;
				int ans_0 = piece_0.second;
				int ans_1 = piece_1.second;
				// 0 を要素少ないほうに
				if (elem_0.size() > elem_1.size()) {
					std::swap(elem_0, elem_1);
				}

				// 内包する場合
				if (inclus(elem_0, elem_1)) {
					// 単純引き算のみ行う
					elem_1 = difference(elem_0, elem_1);
					ans_1 -= ans_0;

					// もし{ A,B,C,... } = 0のときそれらは全て0
					if (ans_1 == 0) {
						// open コマンドに入れることが出来てこの式は破棄できる
						for (int32 yx : elem_1) {
							Vec2 p(yx % H, yx / H);
							command_open.push(p);
							res = true;
						}
					}
					// 単一要素のみ残る場合
					else if (elem_1.size() == 1) {
						int yx = *elem_1.begin();
						Vec2 p(yx % H, yx / H);
						// second == 1 なら flag
						if (ans_1 == 1) {
							command_build_flag.push(p);
							res = true;
						}
						// second == 0 なら open
						else if (ans_1 == 0) {
							command_open.push(p);
							res = true;
						}
					}
					continue;
				}
				// 内包しない場合
				else {
					continue;
					// 共通部分を見つける
					std::set<int32> friend_piece = common(elem_0, elem_1);
					// それぞれから共通部分を削ぐ
					auto conv_0 = difference(friend_piece, elem_0);
					auto conv_1 = difference(friend_piece, elem_1);

					// fpが持ち得る数
					int32 M = std::min((int32)(friend_piece.size()), std::min(ans_0, ans_1));
					// 全通り試してみる
					int ok_ans = -1;
					int unique = 0;
					for (int fp_ans = 0; fp_ans <= M; fp_ans++) {
						if (fp_ans <= ans_0 && ans_0 <= (fp_ans + conv_0.size())
							&& fp_ans <= ans_1 && ans_1 <= (fp_ans + conv_1.size())) {
							unique++;
							ok_ans = fp_ans;
						}
					}
					// 仮に単一以外の状況で成り立つ場合確定とは言えない
					if (unique != 1)continue;

					// この場合成立する
					int cv_ans_0 = ans_0 - ok_ans;
					int cv_ans_1 = ans_1 - ok_ans;

					// cv_anc == 0 のとき、要素群は全て0にできる
					if (cv_ans_0 == 0) {
						// conv_0 は全部解法指令
						for (int32 yx : conv_0) {
							Vec2 p(yx % H, yx / H);
							command_open.push(p);
							res = true;
						}
					}
					// 同じく0なら全部解放
					else if (cv_ans_1 == 0) {
						for (int32 yx : conv_1) {
							Vec2 p(yx % H, yx / H);
							command_open.push(p);
							res = true;
						}
					}
					// fp_ans == 0 なら friend_piece は全開放
					else if (ok_ans == 0) {
						for (int32 yx : friend_piece) {
							Vec2 p(yx % H, yx / H);
							command_open.push(p);
							res = true;
						}
					}
					// cv_ans_0 == conv_0.size() なら全部1
					else if (cv_ans_0 == conv_0.size()) {
						for (int32 yx : conv_0) {
							Vec2 p(yx % H, yx / H);
							command_build_flag.push(p);
							res = true;
						}
					}
					// cv_ans_1 == conv_1.size() なら全部1
					else if (cv_ans_1 == conv_1.size()) {
						for (int32 yx : conv_1) {
							Vec2 p(yx % H, yx / H);
							command_build_flag.push(p);
							res = true;
						}
					}
					// fp_ans == friend_piece なら全部1
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
		// 未解決の座標について考えます
		Vec2 pos = need_think_pos.front();
		need_think_pos.pop();
		Tile& t = field[pos.y][pos.x];

		// 開いてて数字がわかる場合
		Array<Vec2> nx_pos = look_around(pos);

		// 既にフラグが立っている数を数える
		int32 sum_flag = 0;
		// 既に空いているマスを数える
		int32 sum_open = 0;

		int32 sum_closed = 0;


		for (Vec2 p : nx_pos) {
			Tile nx_t = field[p.y][p.x];
			if (nx_t.flag)sum_flag++;
			if (nx_t.open)sum_open++;
			if (!nx_t.open)sum_closed++;
		}


		// 閉マス == 数字 なら全部旗でいい、すでに立ってるなら指令なし
		if (sum_closed == t.num) {
			for (Vec2 p : nx_pos) {
				Tile nx_t = field[p.y][p.x];
				// 開いてない & flag 立ってない場合
				if (!nx_t.open && !nx_t.flag) {
					nx_t.flag = true;
					command_build_flag.push(p);
					res = true;
				}
			}
		}
		// まだ分からない場合はneed_thinkに戻します
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
		// 未解決の座標について考えます
		Vec2 pos = need_think_pos.front();
		need_think_pos.pop();
		Tile& t = field[pos.y][pos.x];

		// マスの数字が分からないなら解決できない
		if (!t.open) {
			continue;
		}

		// 開いてて数字がわかる場合
		Array<Vec2> nx_pos = look_around(pos);

		// 既にフラグが立っている数を数える
		int32 sum_flag = 0;
		// 既に空いているマスを数える
		int32 sum_open = 0;

		int32 sum_closed = 0;


		for (Vec2 p : nx_pos) {
			Tile nx_t = field[p.y][p.x];
			if (nx_t.flag)sum_flag++;
			if (nx_t.open)sum_open++;
			if (!nx_t.open)sum_closed++;
		}


		// num == sum_flag のとき、flagなし閉まってるマス全部開く
		// num == sum_closed + sum_flag でflagなし閉マス全部開ける
		if (sum_flag == t.num) {
			for (Vec2 p : nx_pos) {
				Tile nx_t = field[p.y][p.x];
				// 開いてない & flag 立ってない場合
				if (!nx_t.open && !nx_t.flag) {
					nx_t.open = true;
					command_open.push(p);
					res = true;
				}
			}
		}
		//じゃないなら再考
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
	// そもそも実行してない場合は実行すればいいよね
	if (!thinking)return false;

	// 実行中であれば終わったかどうかを確認したい
	std::future_status status;
	status = task.wait_for(0ms);
	if (status == std::future_status::ready) {
		// 準備が整ってる
		stalemate = task.get();
		// やることないなら死のカウントを増やす
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

