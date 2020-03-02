# include"Game.h"

inline Field::Field(const int32 width, const int32 height, const int32 mines) {
	H = height;
	W = width;
	mine_num = mines;
	reset();
	tex_flag = Texture(Emoji(U"🚩"));
	tex_bomb = Texture(Emoji(U"☢"));
	flag_count = 0;
	for (int i = 0; i < 10; i++)tex_num.push_back(Texture(Emoji(U"{}"_fmt(i))));
	for (int i = 0; i < 10; i++)tr_num.push_back(Texture(Emoji(U"{}"_fmt(i))));
}

void Field::draw() {
	for (int i = 0; i < H; i++) {
		for (int j = 0; j < W; j++) {
			Tile& tile = field[i][j];

			//枠+空マス描画
			field[i][j].rect.drawFrame(0, 1, color[3])
				.draw(color[0]);
			//まだ空いてない場合は濃いの
			if (!tile.open) {
				field[i][j].rect.draw(color[1]);
				//flag描画
				if (field[i][j].flag) {
					tr_flag.drawAt(field[i][j].rect.center());
				}
			}
			//開いてるなら数字
			else {
				if (tile.num != 0) {
					/*font(tile.num)
						.draw(tile.num_Vec2
							, color[3]);*/
					tr_num[tile.num].drawAt(field[i][j].rect.center(), color[3]);
				}
			}

			if (tile.explosion) {
				tr_bomb.drawAt(tile.rect.center());
			}
		}
	}
}

bool Field::mouse_orver() {
	for (int i = 0; i < H; i++)
		for (int j = 0; j < W; j++)
			if (field[i][j].rect.mouseOver())return true;
	return false;
}

Vec2 Field::click_right() {
	for (int i = 0; i < H; i++)
		for (int j = 0; j < W; j++)
			if (field[i][j].rect.rightClicked())return Vec2(j, i);
	return Vec2(-1, -1);
}

Vec2 Field::click_left() {
	for (int i = 0; i < H; i++)
		for (int j = 0; j < W; j++)
			if (field[i][j].rect.leftClicked())return Vec2(j, i);
	return Vec2(-1, -1);
}

void Field::build_flag(Vec2 pos) {
	if (field[pos.y][pos.x].open)return;
	if (field[pos.y][pos.x].flag) { field[pos.y][pos.x].flag = false; flag_count++; }
	else { field[pos.y][pos.x].flag = true; flag_count--; }
}

void Field::crate(Vec2 pos) {
	//pos含め周囲9マスは絶対に爆弾を置かない
	int32 bomb_count = 0;
	int32 bomb_limit = (H + .0) * (W + .0) * (mine_num / 100.0);
	flag_count = bomb_limit;
	while (bomb_count < bomb_limit) {
		int32 x = Random(W - 1);
		int32 y = Random(H - 1);
		if ((std::abs(pos.y - y) > 1 || std::abs(pos.x - x) > 1) && !field[y][x].mine) {
			field[y][x].mine = true;
			for (int dx = -1; dx < 2; dx++)
				for (int dy = -1; dy < 2; dy++)
					if (dx + x >= 0 && dx + x < W && dy + y >= 0 && dy + y < H)
						field[y + dy][x + dx].num += 1;
			bomb_count++;
		}
	}
	//絵文字の大きさ変える
	tr_flag = tex_flag.resized(field[0][0].rect.size * 0.9);
	tr_bomb = tex_bomb.resized(field[0][0].rect.size * 0.9);
	for (int i = 0; i < 10; i++)
		tr_num[i] = tex_num[i].resized(field[0][0].rect.size * 1.1);

}

bool Field::click_open(Vec2& pos) {
	// 旗立ってたら押せない
	if (field[pos.y][pos.x].flag)return false;
	//爆弾で即終了
	if (field[pos.y][pos.x].mine) { return true; }
	//マスなら連鎖起こす
	std::queue<Vec2> q;
	if (!field[pos.y][pos.x].open) {
		q.push(pos);
	}
	//数字で当てはまるなら連鎖起こす
	if (field[pos.y][pos.x].num != 0) {
		//0じゃなくて周囲のflag = num ならflag以外開ける
		int32 flag_count = 0;
		for (int dx = -1; dx < 2; dx++)
			for (int dy = -1; dy < 2; dy++)
				if (dx + pos.x >= 0 && dx + pos.x < W && dy + pos.y >= 0 && dy + pos.y < H)
					flag_count += (field[pos.y + dy][pos.x + dx].flag ? 1 : 0);
		if (flag_count == field[pos.y][pos.x].num) {
			for (int dx = -1; dx < 2; dx++)
				for (int dy = -1; dy < 2; dy++)
					if (dx + pos.x >= 0 && dx + pos.x < W && dy + pos.y >= 0 && dy + pos.y < H)
						if (!field[pos.y + dy][pos.x + dx].flag && !field[pos.y + dy][pos.x + dx].open)
							q.push(Vec2(pos.x + dx, pos.y + dy));
		}
	}

	//BFSで開ける
	while (q.size() != 0) {
		Vec2 p = q.front();
		q.pop();
		//爆弾なら終了
		if (field[p.y][p.x].mine) { pos = p; return true; }
		//既に空いてるなら考えない
		if (field[p.y][p.x].open)continue;
		//爆弾じゃないなら開ける
		field[p.y][p.x].open = true;
		if (field[p.y][p.x].flag) { field[p.y][p.x].flag = false; flag_count++; }

		//0なら周囲9マスに伝染させる
		if (field[p.y][p.x].num == 0) {
			for (int dx = -1; dx < 2; dx++)
				for (int dy = -1; dy < 2; dy++)
					if (dx + p.x >= 0 && dx + p.x < W && dy + p.y >= 0 && dy + p.y < H)
						if (!field[p.y + dy][p.x + dx].open)q.push(Vec2(p.x + dx, p.y + dy));
		}
	}


	return false;
}

int32 Field::get_flag_count() {
	return flag_count;
}

//初期状態に戻します
void Field::reset() {
	int32 MAX = std::max(W, H);
	double xy = (760.0 / (MAX + .0));
	font = Font(xy, Typeface::Bold);
	double _x = (MAX - W + .0) / 2.0 * xy;
	double _y = (MAX - H + .0) / 2.0 * xy;
	field.clear();
	for (int i = 0; i < H; i++) {
		Array<Tile> tiles;
		for (int j = 0; j < W; j++) {
			Tile t;
			t.rect = Rect(10 + _x + xy * j, 10 + _y + xy * i, xy, xy);
			t.num_Vec2 = Vec2(t.rect.x + font.fontSize() / 4, t.rect.y - font.fontSize() / 4);
			tiles.push_back(t);
		}
		field.push_back(tiles);
	}
	flag_count = 0;
}

void Field::reset(int32 w, int32 h, int32 m) {
	W = w;
	H = h;
	mine_num = m;
	reset();
}

//強制的に開けます
void Field::master_open(Vec2 pos) {
	field[pos.y][pos.x].open = true;
}

//指定座標のRectの中心を得ます
Vec2 Field::get_rect_center(Vec2 pos) {
	return field[pos.y][pos.x].rect.center();
}

//誘爆の描画です
void Field::causing_explosion(Circle c) {
	for (int i = 0; i < H; i++) {
		for (int j = 0; j < W; j++) {
			Tile& tile = field[i][j];
			if (tile.mine && tile.rect.intersects(c)) {
				tile.explosion = true;
			}
		}
	}
}

bool Field::success() {
	int32 count = 0;
	int32 sum_mines = 0;
	for (int i = 0; i < H; i++) {
		for (int j = 0; j < W; j++) {
			Tile& tile = field[i][j];
			if (tile.open)count++;
			if (tile.mine)sum_mines++;
		}
	}
	return ((count + sum_mines) == (H * W));
}

bool Field::get_on_flag(Vec2 pos) {
	return field[pos.y][pos.x].flag;
}

bool Field::get_on_open(Vec2 pos) {
	return field[pos.y][pos.x].open;
}

int32 Field::get_on_num(Vec2 pos) {
	return field[pos.y][pos.x].num;
}








bool Game::update() {
	bool fin = false;
	//手の形変えるだけ
	if (field.mouse_orver()) {
		Cursor::RequestStyle(CursorStyle::Hand);
	}

	//クリックされたら
	Vec2 flag_pos = field.click_right();
	Vec2 open_pos = field.click_left();
	//右クリック時の動作
	if (flag_pos.x != -1) {
		field.build_flag(flag_pos);
	}
	//左クリック
	else if (open_pos.x != -1) {
		if (!in_game) {
			//初期生成
			in_game = true;
			field.crate(open_pos);
			sw.start();
		}

		// 開ける動作
		fin = field.click_open(open_pos);
		// 死んだとき
		if (fin) {
			field.master_open(open_pos);
			sw.pause();
			dead = true;
			dead_count = 0;
			dead_pos = open_pos;
		}
		// 解除成功
		else if (field.success()) {
			safe = true;
			fin = true;
			sw.pause();
		}
	}

	else if (rect_setup.leftClicked()) {
		in_config = true;
	}

	return fin;
}

Game::Game(const int32 width, const int32 height, const int32 mine) : field(width, height, mine), ai(width, height, mine) {
	in_game = false;
	in_config = false;
	dead = false;
	safe = false;
	font = Font(50);
	tex_time = Texture(Emoji(U"⏱")).resized(font.fontSize() * 1.2);
	tex_flag = Texture(Emoji(U"🚩")).resized(font.fontSize() * 1.2);
	tex_reset = Texture(Emoji(U"🔄")).resized(font.fontSize() * 1.2);
	tex_ai = Texture(Emoji(U"💉")).resized(font.fontSize() * 1.2);
	tex_setup = Texture(Emoji(U"⚙")).resized(font.fontSize() * 1.2);
	flame = RoundRect(0, 0, 400, 100, 10);
	rect_ai = Rect(960, 350, 100, 100);
	rect_reset = RoundRect(820, 600, 150, 100, 10);
	rect_setup = RoundRect(1070, 600, 150, 100, 10);
	bold = Font(50, Typeface::Bold);
	rect_config_back = RoundRect(50, 50, 200, 100, 10);
	rect_config_enter = RoundRect(1030, 50, 200, 100, 10);
	h = height;
	w = width;
	mines = mine;
	_w = (w - 10) / 40.0;
	_h = (h - 10) / 40.0;
	_m = (mines - 1) / 39.0;
	rect_config_frame = RoundRect(0, 0, 1200, 150, 10);
	use_ai = false;
}

void Game::draw() {
	field.draw();
	String colon = U":";
	// Timer
	flame.movedBy(Vec2(820, 50)).drawFrame(0, 5, color[3]);
	tex_time.draw(Vec2(840, 75));
	font(colon).draw(Vec2(920, 65), color[3]);
	if (safe) {
		const int32 t = Scene::Time();
		font(std::round(sw.sF() * 100) / 100).draw(Vec2(980, 65), ((t % 2) >= 1) ? color[3] : Palette::Pink);
	}
	else font(std::round(sw.sF() * 100) / 100).draw(Vec2(980, 65), color[3]);

	// Flag
	flame.movedBy(Vec2(820, 200)).drawFrame(0, 5, color[3]);
	tex_flag.draw(Vec2(840, 225));
	font(colon).draw(Vec2(920, 215), color[3]);
	font(field.get_flag_count()).draw(Vec2(980, 215), color[3]);

	// dead stage
	if (dead && dead_count < 60)dead_stage();

	//// AI
	//// ----- 公開時はこれいらない ---
	//if (!dead && !safe && rect_ai.mouseOver()) flame.movedBy(Vec2(820, 350)).drawFrame(0, 5, color[3]).draw(color[3]);
	//else flame.movedBy(Vec2(820, 350)).drawFrame(0, 5, color[3]).draw(color[1]);
	//rect_ai.draw(ColorF(0, 0));
	//tex_ai.drawAt(rect_ai.center());
	//// ----- 公開時はこれいらない ---


	// Reset
	if (rect_reset.mouseOver()) rect_reset.drawFrame(0, 5, color[3]).draw(color[3]);
	else rect_reset.drawFrame(0, 5, color[3]).draw(color[1]);
	tex_reset.rotated(-Scene::Time() * 30_deg).drawAt(rect_reset.center());
	// Setup
	if (!dead && !safe && rect_setup.mouseOver()) rect_setup.drawFrame(0, 5, color[3]).draw(color[3]);
	else rect_setup.drawFrame(0, 5, color[3]).draw(color[1]);
	tex_setup.drawAt(rect_setup.center());
}

// 死んだときの演出はここ
void Game::dead_stage() {
	dead_circle = Circle(field.get_rect_center(dead_pos), std::pow(1.13, dead_count));
	field.causing_explosion(dead_circle);
	dead_circle.drawFrame(0, (10 * (dead_count / 8.0)), ColorF(255, 255, 255, 1));
	dead_count++;
}

bool Game::clicked_reset() {
	return rect_reset.leftClicked();
}

bool Game::clicked_setup() {
	return in_config;
}

bool Game::clicked_ai() {
	// ----- 公開時はこれいらない ---
	return false;
	if (rect_ai.leftClicked()) {
		reset();
		ai.reset(h, w, mines);
		use_ai = true;
	}
	return use_ai;
}

void Game::config() {
	// config GUI を作る
	if (rect_config_back.mouseOver()) {
		rect_config_back.drawFrame(0, 5, color[1]).draw(color[3]);
		bold(U"Back").drawAt(rect_config_back.center(), color[1]);
	}
	else {
		rect_config_back.drawFrame(0, 5, color[3]).draw(color[1]);
		bold(U"Back").drawAt(rect_config_back.center(), color[3]);
	}
	if (rect_config_enter.mouseOver()) {
		rect_config_enter.drawFrame(0, 5, color[1]).draw(color[3]);
		bold(U"Enter").drawAt(rect_config_enter.center(), color[1]);
	}
	else {
		rect_config_enter.drawFrame(0, 5, color[3]).draw(color[1]);
		bold(U"Enter").drawAt(rect_config_enter.center(), color[3]);
	}

	// Width
	rect_config_frame.movedBy(Vec2(40, 200)).drawFrame(0, 5, color[3]);
	SimpleGUI::Slider(_w, Vec2(350, 260), 850);
	bold(U"幅 : {:>3.0f}"_fmt(10 + ceil(40 * _w))).draw(100, 240, color[3]);

	// Height
	rect_config_frame.movedBy(Vec2(40, 400)).drawFrame(0, 5, color[3]);
	SimpleGUI::Slider(_h, Vec2(350, 460), 850);
	bold(U"高さ : {:>3.0f}"_fmt(10 + ceil(40 * _h))).draw(80, 440, color[3]);

	// Mines
	rect_config_frame.movedBy(Vec2(40, 600)).drawFrame(0, 5, color[3]);
	SimpleGUI::Slider(_m, Vec2(350, 660), 850);
	bold(U"割合 : {:>2.0f}%"_fmt(1 + ceil(39 * _m))).draw(75, 640, color[3]);

	if (rect_config_back.leftClicked()) {
		in_config = false;
	}
	else if (rect_config_enter.leftClicked()) {
		h = 10 + ceil(40 * _h);
		_h = ((h - 10) / 40.0);
		w = 10 + ceil(40 * _w);
		_w = ((w - 10) / 40.0);
		mines = 1 + ceil(39 * _m);
		_m = ((mines - 1) / 39.0);
		TextWriter tw(U"config.ini");
		tw << U"Width = " << w;
		tw << U"Height = " << h;
		tw << U"Mine = " << mines;
		tw.close();
		field.reset(w, h, mines);
		reset();
		in_config = false;
	}
}

bool Game::ai_update() {
	// 盤面更新がない状態で考えるから詰まる
	// 前回の考え中に指令があったならそっち優先させて全部消化するまで待つ

	//まず旗立てが出来るならそれをやる
	if (ai.exist_command_build_flag()) {
		// 開ける座標貰って
		Vec2 pos = ai.ai_build_flag();
		// 旗立てる
		if (!field.get_on_flag(pos))
			field.build_flag(pos);
	}
	else if (ai.exist_command_open()) {
		// 開ける座標貰って
		Vec2 pos = ai.ai_open();
		// もし初手なら盤面生成させる
		if (!in_game) {
			//初期生成
			in_game = true;
			field.crate(pos);
			sw.start();
		}
		// 指示されたマスが開いてない場合
		if (!field.get_on_open(pos)) {
			// 開ける
			bool fin = field.click_open(pos);
			if (fin) {
				field.master_open(pos);
				sw.pause();
				dead = true;
				use_ai = false;
				dead_count = 0;
				dead_pos = pos;
				return false;
			}
			// 解除成功
			else if (field.success()) {
				safe = true;
				fin = true;
				use_ai = false;
				sw.pause();
				return false;
			}
		}
	}

	// ゲーム終わってたら即終わらせる
	if (!use_ai)return use_ai;

	// 指令がまだあるならそれ消化しきる
	if (ai.exist_command_build_flag() || ai.exist_command_open())return true;

	if (!ai.is_thinking()) {
		// 実行してない状態なら並列実行する
		ai.run(field);
	}
	else {
		// 実行中です
	}


	return use_ai;
}

void Game::reset() {
	//終了
	sw.reset();
	in_game = false;
	dead = false;
	safe = false;
	field.reset();
}