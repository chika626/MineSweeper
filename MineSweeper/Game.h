#pragma once
# include <Siv3D.hpp>
# include <queue>
# include <future>
# include <map>

const Color color[4] = { Color(239, 252, 239),Color(204, 237, 210) ,Color(148, 211, 172),Color(101, 92, 86) };


class Field {
private:
	struct Tile {
		int32 num = 0;
		bool mine = false;
		bool flag = false;
		bool open = false;
		bool explosion = false;
		Rect rect;
		Vec2 num_Vec2;
	};
	Texture tex_flag;
	Texture tex_bomb;
	TextureRegion tr_flag;
	TextureRegion tr_bomb;
	Font font;
	Array<Texture> tex_num;
	Array<TextureRegion> tr_num;
	Array<Array<Tile>> field;
	int32 H, W, mine_num;
	int32 flag_count;
public:
	Field(const int32 width, const int32 height, const int32 mine);
	void draw();
	bool mouse_orver();
	Vec2 click_right();
	Vec2 click_left();

	void build_flag(Vec2 pos);
	void crate(Vec2 pos);
	bool click_open(Vec2& pos);

	int32 get_flag_count();

	void reset();
	void reset(int32 w, int32 h, int32 m);
	void master_open(Vec2 pos);

	Vec2 get_rect_center(Vec2 pos);
	void causing_explosion(Circle c);

	bool success();

	bool get_on_flag(Vec2 pos);
	bool get_on_open(Vec2 pos);
	int32 get_on_num(Vec2 pos);
};


class AI {
private:
	struct Tile {
		bool flag = false;
		bool open = false;
		int32 num = 0;
		int32 probability_flag = 0;
		bool need_think = true;
	};
	Array<Array<Tile>> field;
	int32 W, H, M;
	int32 sum_mines;
	int32 found_mines;

	// まだ考えてない座標群
	// これ全部毎回考えるの意味わかんないから 開いてるマスだけ追加していく
	std::queue<Vec2> unsolved_pos;
	std::queue<Vec2> need_think_pos;

	// 開いてないから考えられない座標だよ
	std::set<Vec2> not_open_pos;
	// 開いてるけど自明じゃない座標だよ 論理が通用する可能性
	std::set<Vec2> open_pos;

	// 溜まっていく指令
	std::queue<Vec2> command_open;
	std::queue<Vec2> command_build_flag;

	bool in_game;

	// 非同期で考えさせるためのタスク
	std::future<bool> task;
	bool thinking;
	// 手詰まりサイン　乱択をするためのそれ
	int32 death_count;
	bool stalemate;

	// 論理のために最初にやる謎の計算
	Array<Array<Array<Array<int>>>> comb_master;

public:
	AI(int32 w, int32 h, int32 m);

	//現在の盤面状況に対しての手を返させる
	void run(Field f);
	Vec2 ai_open();
	Vec2 ai_build_flag();

	bool think();
	bool logic();
	bool logic2();

	void random();
	bool suggest_build_flag();
	bool suggest_open();

	void get_update(Field f);
	void reset(int32 w, int32 h, int32 m);
	Array<Vec2> look_around(Vec2 pos);

	bool is_thinking();
	bool is_stalemate();
	bool exist_command_open();
	bool exist_command_build_flag();
};



class Game {
private:
	Stopwatch sw;
	bool in_game; // true ゲーム中
	Field field;
	Font font;

	TextureRegion tex_flag;
	TextureRegion tex_time;
	TextureRegion tex_reset;
	TextureRegion tex_ai;
	TextureRegion tex_setup;
	RoundRect flame;
	Rect rect_ai;
	RoundRect rect_reset;
	RoundRect rect_setup;

	bool dead;
	int32 dead_count;
	Vec2 dead_pos;

	bool safe;

	bool use_ai;

	bool in_config;
	Font bold;
	RoundRect rect_config_back;
	RoundRect rect_config_enter;
	int32 h, w, mines;
	double _h, _w, _m;
	RoundRect rect_config_frame;

	AI ai;

public:
	Game(const int32 width, const int32 height, const int32 mine);
	bool update();
	void draw();
	void reset();

	Circle dead_circle;
	void dead_stage();

	bool clicked_reset();
	bool clicked_setup();
	bool clicked_ai();

	void config();

	bool ai_update();
};
