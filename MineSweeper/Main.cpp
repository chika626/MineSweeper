# include "Game.h"
#include <stdio.h>
#include <tchar.h>
#include <iostream>
#include <fstream>
#include <string>
#include <windows.h>

void Main()
{
	Scene::SetBackground(color[0]);
	Scene::SetLetterbox(color[0]);
	Window::Resize(1280, 780);
	Scene::Resize(1280, 780);
	Window::SetStyle(WindowStyle::Sizable);
	Window::SetTitle(U"MineSweeper");

	const INIData ini(U"config.ini");
	const int32 width = Parse<int32>(ini[U"Width"]);
	const int32 height = Parse<int32>(ini[U"Height"]);
	const int32 mine = Parse<int32>(ini[U"Mine"]);

	Game game = Game::Game(width, height, mine);
	bool fin = false;
	bool use_ai = false;

	while (System::Update())
	{
		//ai側の操作
		if (use_ai) {
			use_ai = game.ai_update();
			game.draw();
			if (!use_ai)Print << U"AI End";
		}
		//人間側の操作
		else {
			if (!game.clicked_setup() && !fin)fin = game.update();
			//リセット押したら状態を戻す
			if (!game.clicked_setup() && game.clicked_reset()) { game.reset(); fin = false; }
			//AIボタン押されたら制御を渡します
			if (game.clicked_ai()) { use_ai = true; Print << U"AI Start"; }
			//設定ボタンで描画が入れ替わる
			if (game.clicked_setup()) { game.config(); }
			else game.draw();
		}
	}
}