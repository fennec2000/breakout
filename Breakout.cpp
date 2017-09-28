// Breakout.cpp: A program using the TL-Engine

#include <TL-Engine.h>	// TL-Engine include file and namespace
#include <fstream> // read file - used to read map files
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <float.h> // FLT_EPSILON
using namespace tle;

enum gameState { playing = 0, paused, mainmenu, over }; // all the states the game can enter
enum bounceDirrection { horizontal, vertical }; // direction to bounce the ball on
enum gameMode { single, coop, vs}; // game mode
enum direction { North, South, East, West }; // used in collision detection


// Global
I3DEngine* myEngine = New3DEngine(kTLX);
float frameTime;
float countDown;
const float gameRate = 1.0f; // Rate the game runs at // used for tweak game speed
const int screenWidth = 1280;
const int screenHeight = 720;
int level = 0;
gameState currentGameState = gameState::playing;

// Variables
const int playerHeight = 26;
const int playerWidth = 86;
const int playerSpeed = 250;

// brick variables
const int brickOffsetX = 448;
const int brickOffsetY = 64;
const int brickHeight = 32;
const int brickWidth = 64;
const int brickMaxGridWidth = 6;
const int brickMaxGridHeight = 16;
const char *brickNames[]{ "Block2.png", "Block3.png", "Block4.png", "Block5.png", "Block6.png", "Block7.png" };
const int unbreakableBrickSize = 32; // brick is square
const int totalUbreakable = 720 / unbreakableBrickSize + 1;
int brickPower[brickMaxGridHeight][brickMaxGridWidth] = { 0 }; // bricks start from top left

// keybindings
const EKeyCode keyClose = Key_Escape;
const EKeyCode keyPlayerLeft = Key_Left;
const EKeyCode keyPlayerRight = Key_Right;
const EKeyCode keyPlayerBoost = Key_Shift;
const EKeyCode keyContinue = Key_Return;

// ball
const int ballRadius = 8;
const int ballSpeed = 250;

// Models
ISprite* player;
ISprite* brick[brickMaxGridHeight][brickMaxGridWidth]; // bricks start from top left
ISprite* unbreakable[3][totalUbreakable];
ISprite* arrows[2];

// Text
IFont* myFont;
IFont* myTitleFont;

class gameStats
{
private:
	int score = 0;
	float time = 0.0f;
	int lives = 3;
public:
	void AddTime() { time += frameTime; };
	void AddScore(int points) { score += points; };
	void LoseLife() { lives--; };
};

// ball entity // requires x and y to spawn in constructor
class ballEntity
{
private:
	ISprite* ball;
	float oldX, oldY;
	float xVelocity = 1.0f, yVelocity = 1.0f;
public:
	ballEntity(float x, float y) { ball = myEngine->CreateSprite("TinyBall.png", x, y); };
	void Move();
	void Bounce(bounceDirrection dir);
	float GetX() { return ball->GetX(); };
	float GetY() { return ball->GetY(); };
	void StepBack() { ball->SetPosition(oldX, oldY); };
	void PushUp(int a) { ball->MoveY(a); };
};

void ballEntity::Bounce(bounceDirrection dir)
{
	if (dir == bounceDirrection::horizontal)
		yVelocity = -yVelocity;
	else
		xVelocity = -xVelocity;
}

void ballEntity::Move()
{
	oldX = ball->GetX();
	oldY = ball->GetY();
	ball->Move(xVelocity * frameTime * ballSpeed * gameRate, yVelocity * frameTime * ballSpeed * gameRate);
}

// Collision Dectection

// Sides
bool blockCollision(float ballPosX, float ballPosY, float ballRadius,
	float blockX, float blockY, float blockHeight, float blockWidth)
{
	float blockMinX = blockX;
	float blockMaxX = blockX + blockWidth;
	float blockMinY = blockY;
	float blockMaxY = blockY + blockHeight;

	return(ballPosX >= blockMinX - (ballRadius * 2) && ballPosX <= blockMaxX
		&& ballPosY >= blockMinY - (ballRadius * 2) && ballPosY <= blockMaxY);
}

// Wall collision
bool wallCollision(float ballPosX, float ballPosY, float ballRadius, float wallLine, direction dir)
{
	if (dir == direction::North)
	{
		return(ballPosY <= wallLine);
	}
	else if (dir == direction::East)
	{
		return(ballPosX <= wallLine);
	}
	else if (dir == direction::South)
	{
		return(ballPosY >= wallLine - (2 * ballRadius));
	}
	else if (dir == direction::West)
	{
		return(ballPosX >= wallLine - (2 * ballRadius));
	}
}

void LoadMap()
{
	level++;

	while (true)
	{
		// Load map
		string mapFileName = "maps/level" + std::to_string(level) + ".map"; // get map anme
		ifstream mapfile(mapFileName);
		string line;

		if (mapfile.is_open())
		{
			while (getline(mapfile, line))// for each line in map file
			{
				vector<int> vect;
				stringstream ss(line);

				int i;

				while (ss >> i) // for each variable (only first 3)
				{
					vect.push_back(i);

					if (ss.peek() == ',' || ss.peek() == ' ') // ignore ',' and ' ' - allows for mistakes in map file and still load
						ss.ignore();
				}
				if (vect.at(0) >= 0 && vect.at(0) <= brickMaxGridHeight && // check the height is valid
					vect.at(1) >= 0 && vect.at(1) <= brickMaxGridWidth && // check the width is valid
					vect.at(2) >= 1 && vect.at(2) <= sizeof(brickNames)) // check the strenght is valid
				{
					// set up brick
					brick[vect.at(0)][vect.at(1)] = myEngine->CreateSprite(brickNames[vect.at(2) - 1], vect.at(1) * brickWidth + brickOffsetX, vect.at(0) * brickHeight + brickOffsetY);
					brickPower[vect.at(0)][vect.at(1)] = vect.at(2);
				}
			}
			mapfile.close();
			countDown = 3.0f;
			break;
		}
		else // go back to level 1
			level = 1;
	}
	
}

void main()
{
	// Create a 3D engine (using TLX engine here) and open a window for it
	myEngine->StartWindowed(screenWidth, screenHeight);

	// Add default folder for meshes and other media
	myEngine->AddMediaFolder("C:\\ProgramData\\TL-Engine\\Media"); // Default location
	myEngine->AddMediaFolder("sprites"); // Extras

	/**** Set up your scene here ****/

	// Unbreakable bricks
	// left side
	for (int i = 0; i < totalUbreakable; i++)
	{
		unbreakable[0][i] = myEngine->CreateSprite("Unbreakable.png", brickOffsetX - unbreakableBrickSize - unbreakableBrickSize, unbreakableBrickSize * i);
	}
	// top
	for (int i = 0; i < brickMaxGridWidth * brickWidth / unbreakableBrickSize + 2; i++) // +2 to add space on each side of the bricks
	{
		unbreakable[0][i] = myEngine->CreateSprite("Unbreakable.png", unbreakableBrickSize * (i - 1) + brickOffsetX, 0);
	}
	// right side
	for (int i = 0; i < totalUbreakable; i++)
	{
		unbreakable[2][i] = myEngine->CreateSprite("Unbreakable.png", brickOffsetX + brickMaxGridWidth * brickWidth + unbreakableBrickSize, unbreakableBrickSize * i);
	}

	// player
	player = myEngine->CreateSprite("Bat.png", (screenWidth - playerWidth) / 2 - 5, screenHeight - playerHeight);

	// Ball
	const int ballRadius = 8;
	ballEntity ball1(player->GetX() + playerWidth / 2 - ballRadius, player->GetY() - 2 * ballRadius);

	// Game stats
	gameStats thisGameStats;

	// Font
	myFont = myEngine->LoadFont("Consolas", 36);
	myTitleFont = myEngine->LoadFont("Consolas", 72);

	// Arrows
	bool arrowActive[2] = { false, false };

	// Load map
	LoadMap();

	// The main game loop, repeat until engine is stopped
	while (myEngine->IsRunning())
	{
		if (currentGameState == gameState::playing)
		{
			// timer
			frameTime = myEngine->Timer();
			thisGameStats.AddTime();

			// Draw the scene
			myEngine->DrawScene();

			if (countDown > FLT_EPSILON)
			{
				myFont->Draw(to_string(int(countDown + 0.5f)), screenWidth / 2, screenHeight / 2, kRed, kCentre, kVCentre);
				countDown -= frameTime;
			}
			else
			{
				/**** Update your scene each frame here ****/

				// Move ball
				ball1.Move();

				// Collision
				// ball and player
				if (blockCollision(ball1.GetX(), ball1.GetY(), ballRadius, player->GetX(), player->GetY(), playerHeight, playerWidth))
				{
					ball1.StepBack();

					if ((ball1.GetX() + ballRadius >= player->GetX() && ball1.GetX() + ballRadius <= player->GetX() + playerWidth) &&
						(ball1.GetY() + ballRadius <= player->GetY() || ball1.GetY() + ballRadius >= player->GetY() + playerHeight))
					{
						ball1.Bounce(horizontal);
					}

					else if ((ball1.GetY() + ballRadius >= player->GetY() && ball1.GetY() + ballRadius <= player->GetY() + playerHeight) &&
						(ball1.GetX() + ballRadius <= player->GetX() || ball1.GetX() + ballRadius >= player->GetX() + playerWidth))
					{
						while ((ball1.GetY() + ballRadius >= player->GetY() && ball1.GetY() + ballRadius <= player->GetY() + playerHeight) &&
							(ball1.GetX() + ballRadius <= player->GetX() || ball1.GetX() + ballRadius >= player->GetX() + playerWidth))
						{
							ball1.PushUp(-1);
						}
						ball1.PushUp(-8);
					}
					else
					{
						ball1.Bounce(horizontal);
						ball1.Bounce(vertical);
					}
				}

				// ball and block
				for (int i = 0; i < brickMaxGridHeight; i++)// row
				{
					for (int j = 0; j < brickMaxGridWidth; j++) // col
					{
						if (brickPower[i][j] > 0) // if brick exists
						{
							// check for collision
							if (blockCollision(ball1.GetX(), ball1.GetY(), ballRadius, brick[i][j]->GetX(), brick[i][j]->GetY(), brickHeight, brickWidth))
							{
								ball1.StepBack();

								if ((ball1.GetX() + ballRadius >= brick[i][j]->GetX() && ball1.GetX() + ballRadius <= brick[i][j]->GetX() + brickWidth) &&
									(ball1.GetY() + ballRadius <= brick[i][j]->GetY() || ball1.GetY() + ballRadius >= brick[i][j]->GetY() + brickHeight))
								{
									ball1.Bounce(horizontal);
								}

								else if ((ball1.GetY() + ballRadius >= brick[i][j]->GetY() && ball1.GetY() + ballRadius <= brick[i][j]->GetY() + brickHeight) &&
									(ball1.GetX() + ballRadius <= brick[i][j]->GetX() || ball1.GetX() + ballRadius >= brick[i][j]->GetX() + brickWidth))
								{
									ball1.Bounce(vertical);
								}
								else
								{
									ball1.Bounce(horizontal);
									ball1.Bounce(vertical);
								}

								// brick damage
								brickPower[i][j]--;
								float x = brick[i][j]->GetX(), y = brick[i][j]->GetY();
								brick[i][j]->~ISprite();

								if (brickPower[i][j] > 0)
								{
									brick[i][j] = myEngine->CreateSprite(brickNames[brickPower[i][j] - 1], x, y);
								}
							}
						}
					}
				}

				// ball and wall 
				if (wallCollision(ball1.GetX(), ball1.GetY(), ballRadius, brickOffsetX - unbreakableBrickSize, direction::East))
				{
					ball1.StepBack();
					ball1.Bounce(vertical);

				}
				if (wallCollision(ball1.GetX(), ball1.GetY(), ballRadius, brickOffsetX + brickMaxGridWidth * brickWidth + unbreakableBrickSize, direction::West))
				{
					ball1.StepBack();
					ball1.Bounce(vertical);
				}

				if (wallCollision(ball1.GetX(), ball1.GetY(), ballRadius, brickOffsetY - unbreakableBrickSize, direction::North))
				{
					ball1.StepBack();
					ball1.Bounce(horizontal);
				}

				// general keys
				if (myEngine->KeyHit(keyClose))
				{
					currentGameState = gameState::paused;
				}

				// player keybindings
				if (myEngine->KeyHeld(keyPlayerLeft))
				{
					// Calculated valuve of the player movement
					float playerTravel = -playerSpeed * frameTime * gameRate;

					if (myEngine->KeyHeld(keyPlayerBoost))
					{
						playerTravel *= 2;
					}

					player->MoveX(playerTravel);
				}
				else if (myEngine->KeyHeld(keyPlayerRight))
				{
					// Calculated valuve of the player movement
					float playerTravel = playerSpeed * frameTime * gameRate;

					if (myEngine->KeyHeld(keyPlayerBoost))
					{
						playerTravel *= 2;
					}

					player->MoveX(playerTravel);
				}
			}
		}

		else if (currentGameState == gameState::paused)
		{
			myEngine->DrawScene();
			myFont->Draw("Paused", screenWidth / 2, screenHeight / 2, kRed, kCentre, kVCentre);
			myFont->Draw("Press Enter to continue", screenWidth / 2, screenHeight / 2 + 30, kRed, kCentre, kVCentre);
			myFont->Draw("Press Escape to quit", screenWidth / 2, screenHeight / 2 + 60, kRed, kCentre, kVCentre);

			// general keys
			if (myEngine->KeyHit(keyClose))
			{
				myEngine->Stop();
			}

			if (myEngine->KeyHit(keyContinue))
			{
				currentGameState = gameState::playing;
			}
		}

		else if (currentGameState == gameState::mainmenu)
		{
			myEngine->DrawScene();

			// Text
			myTitleFont->Draw("Breakout", screenWidth / 2, screenHeight / 3, kRed, kCentre, kVCentre);
			myFont->Draw("New Game", screenWidth / 2, screenHeight / 2, kRed, kCentre, kVCentre);
			myFont->Draw("Quit", screenWidth / 2, screenHeight / 2 + 30, kRed, kCentre, kVCentre);

			// Arrows
			if (arrowActive[0] == false)
			{
				//arrows[0] = myEngine->CreateSprite("")
			}
		}

		else if (currentGameState == gameState::over)
		{

		}

	}

	// Delete the 3D engine now we are finished with it
	myEngine->Delete();
}
