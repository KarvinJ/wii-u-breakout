#include "sdl_starter.h"
#include "sdl_assets_loader.h"
#include <unistd.h>
#include <romfs-wiiu.h>
#include <whb/proc.h>
#include <vector>

SDL_Window *window = nullptr;
SDL_Renderer *renderer = nullptr;
SDL_GameController *controller = nullptr;

bool isGamePaused;
int shouldCloseTheGame = 0;

SDL_Texture *pauseGameTexture = nullptr;
SDL_Rect pauseGameBounds;

TTF_Font *font = nullptr;

SDL_Texture *scoreTexture = nullptr;
SDL_Rect scoreBounds;

SDL_Texture *liveTexture = nullptr;
SDL_Rect liveBounds;

Mix_Chunk *collisionSound = nullptr;
Mix_Chunk *collisionWithPlayerSound = nullptr;

SDL_Rect player = {SCREEN_WIDTH / 2, SCREEN_HEIGHT - 32, 74, 16};

int playerScore;
int playerLives = 2;
int playerSpeed = 800;

SDL_Rect ball = {SCREEN_WIDTH / 2 - 20, SCREEN_HEIGHT / 2 - 20, 20, 20};

int ballVelocityX = 425;
int ballVelocityY = 425;

bool isAutoPlayMode = true;

typedef struct
{
    SDL_Rect bounds;
    bool isDestroyed;
    int points;
} Brick;

std::vector<Brick> createBricks()
{
    std::vector<Brick> bricks;
    bricks.reserve(200);

    int brickPoints = 10;
    int positionX;
    int positionY = 50;

    for (int row = 0; row < 10; row++)
    {
        positionX = 0;

        for (int column = 0; column < 20; column++)
        {
            Brick actualBrick = {{positionX, positionY, 60, 20}, false, brickPoints};

            bricks.push_back(actualBrick);
            positionX += 64;
        }

        brickPoints--;
        positionY += 22;
    }

    return bricks;
}

std::vector<Brick> bricks = createBricks();

void quitGame()
{
    Mix_HaltChannel(-1);
    IMG_Quit();
    Mix_CloseAudio();
    TTF_Quit();
    Mix_Quit();
    SDL_Quit();
    romfsExit();
}

void handleEvents()
{
    SDL_Event event;

    while (SDL_PollEvent(&event))
    {
        if (event.type == SDL_QUIT)
        {
            shouldCloseTheGame = 1;
        }

        if (event.type == SDL_JOYBUTTONDOWN)
        {
            if (event.jbutton.button == BUTTON_MINUS)
            {
                shouldCloseTheGame = 1;
            }

            if (event.jbutton.button == BUTTON_PLUS)
            {
                isGamePaused = !isGamePaused;
                Mix_PlayChannel(-1, collisionWithPlayerSound, 0);
            }

            if (event.jbutton.button == BUTTON_A)
            {
                isAutoPlayMode = !isAutoPlayMode;
                Mix_PlayChannel(-1, collisionSound, 0);
            }
        }
    }
}

void update(float deltaTime)
{
    if (isAutoPlayMode && ball.x < SCREEN_WIDTH - player.w)
    {
        player.x = ball.x;
    }

    if (SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_DPAD_LEFT) && player.x > 0)
    {
        player.x -= playerSpeed * deltaTime;
    }

    else if (SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_DPAD_RIGHT) && player.x < SCREEN_WIDTH - player.w)
    {
        player.x += playerSpeed * deltaTime;
    }

    if (ball.y > SCREEN_HEIGHT + ball.h)
    {
        ball.x = SCREEN_WIDTH / 2 - ball.w;
        ball.y = SCREEN_HEIGHT / 2 - ball.h;

        ballVelocityX *= -1;

        if (playerLives > 0)
        {
            playerLives--;

            std::string livesString = "lives: " + std::to_string(playerLives);

            updateTextureText(liveTexture, livesString.c_str(), font, renderer);
        }
    }

    if (ball.x < 0 || ball.x > SCREEN_WIDTH - ball.w)
    {
        ballVelocityX *= -1;
        Mix_PlayChannel(-1, collisionSound, 0);
    }

    if (SDL_HasIntersection(&player, &ball) || ball.y < 0)
    {
        ballVelocityY *= -1;
        Mix_PlayChannel(-1, collisionWithPlayerSound, 0);
    }

    for (auto actualBrick = bricks.begin(); actualBrick != bricks.end();)
    {
        if (!actualBrick->isDestroyed && SDL_HasIntersection(&actualBrick->bounds, &ball))
        {
            ballVelocityY *= -1;
            actualBrick->isDestroyed = true;

            playerScore += actualBrick->points;

            std::string scoreString = "score: " + std::to_string(playerScore);

            updateTextureText(scoreTexture, scoreString.c_str(), font, renderer);

            Mix_PlayChannel(-1, collisionSound, 0);
        }

        if (actualBrick->isDestroyed)
        {
            bricks.erase(actualBrick);
        }
        else
        {
            actualBrick++;
        }
    }

    ball.x += ballVelocityX * deltaTime;
    ball.y += ballVelocityY * deltaTime;
}

void render()
{
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);

    SDL_QueryTexture(scoreTexture, NULL, NULL, &scoreBounds.w, &scoreBounds.h);
    scoreBounds.x = 300;
    scoreBounds.y = scoreBounds.h / 2 - 10;
    SDL_RenderCopy(renderer, scoreTexture, NULL, &scoreBounds);

    SDL_QueryTexture(liveTexture, NULL, NULL, &liveBounds.w, &liveBounds.h);
    liveBounds.x = 800;
    liveBounds.y = liveBounds.h / 2 - 10;
    SDL_RenderCopy(renderer, liveTexture, NULL, &liveBounds);

    if (isGamePaused)
    {
        SDL_RenderCopy(renderer, pauseGameTexture, NULL, &pauseGameBounds);
    }

    SDL_SetRenderDrawColor(renderer, 0, 255, 255, 255);

    for (Brick brick : bricks)
    {
        if (!brick.isDestroyed)
        {
            SDL_RenderFillRect(renderer, &brick.bounds);
        }
    }

    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);

    SDL_RenderFillRect(renderer, &player);
    SDL_RenderFillRect(renderer, &ball);

    SDL_RenderPresent(renderer);
}

int main(int argc, char **argv)
{
    WHBProcInit();
    romfsInit();
    chdir("romfs:/");

    window = SDL_CreateWindow("Breakout", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, SCREEN_WIDTH, SCREEN_HEIGHT, SDL_WINDOW_SHOWN);
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);

    if (startSDL(window, renderer) > 0)
    {
        return 1;
    }

    SDL_JoystickEventState(SDL_ENABLE);
    SDL_JoystickOpen(0);

    controller = SDL_GameControllerOpen(0);

    font = TTF_OpenFont("fonts/LeroyLetteringLightBeta01.ttf", 36);

    updateTextureText(scoreTexture, "score: 0", font, renderer);
    updateTextureText(liveTexture, "lives: 2", font, renderer);

    updateTextureText(pauseGameTexture, "Game Paused", font, renderer);

    SDL_QueryTexture(pauseGameTexture, NULL, NULL, &pauseGameBounds.w, &pauseGameBounds.h);
    pauseGameBounds.x = SCREEN_WIDTH / 2 - pauseGameBounds.w / 2;
    pauseGameBounds.y = SCREEN_HEIGHT / 2 - pauseGameBounds.h / 2;

    collisionSound = loadSound("sounds/pop1.wav");
    collisionWithPlayerSound = loadSound("sounds/pop2.wav");

    Uint32 previousFrameTime = SDL_GetTicks();
    Uint32 currentFrameTime = previousFrameTime;
    float deltaTime = 0.0f;

    while (!shouldCloseTheGame && WHBProcIsRunning())
    {
        currentFrameTime = SDL_GetTicks();
        deltaTime = (currentFrameTime - previousFrameTime) / 1000.0f;
        previousFrameTime = currentFrameTime;

        SDL_GameControllerUpdate();

        handleEvents();

        if (!isGamePaused)
        {
            update(deltaTime);
        }

        render();
    }

    quitGame();
}
