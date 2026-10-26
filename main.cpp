#include "raylib.h"
#include "raymath.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

// ===========================================================================
//  Underwater — Android build
//  Jane Mik — плоский биллборд-спрайт в 3D-мире
//  Управление: левый виртуальный джойстик = движение,
//              свайп по правой половине экрана = вращение камеры
// ===========================================================================

static const float ISLAND_RADIUS  = 12.0f;
static const float PLAYER_SPEED   = 6.0f;
static const float PLAYER_HEIGHT  = 1.8f;
static const float SPRITE_WIDTH   = 1.6f;
static const float SPRITE_HEIGHT  = 2.2f;

// ---------------------------------------------------------------------------
//  Виртуальный джойстик (левый нижний угол)
// ---------------------------------------------------------------------------
struct VirtualJoystick {
    Vector2 basePos;     // центр джойстика в экранных координатах
    Vector2 knobPos;     // текущее положение «шляпки»
    float   baseRadius;  // радиус базы
    float   knobRadius;  // радиус шляпки
    int     touchId;     // ID пальца, который держит джойстик (-1 = не активен)
    bool    active;
    Vector2 delta;       // нормализованный вектор направления (-1..1)
};

static void InitJoystick(VirtualJoystick* js, Vector2 basePos, float baseRadius, float knobRadius)
{
    js->basePos    = basePos;
    js->knobPos    = basePos;
    js->baseRadius = baseRadius;
    js->knobRadius = knobRadius;
    js->touchId    = -1;
    js->active     = false;
    js->delta      = (Vector2){ 0.0f, 0.0f };
}

static void UpdateJoystick(VirtualJoystick* js)
{
    int touchCount = GetTouchPointCount();

    // Начало касания — ищем палец, попавший в зону джойстика
    if (!js->active)
    {
        for (int i = 0; i < touchCount; i++)
        {
            Vector2 p = GetTouchPosition(i);
            float dx = p.x - js->basePos.x;
            float dy = p.y - js->basePos.y;
            if (dx*dx + dy*dy <= js->baseRadius * js->baseRadius * 1.5f)
            {
                js->active  = true;
                js->touchId = GetTouchPointId(i);
                js->knobPos = p;
                break;
            }
        }
    }

    // Отслеживание активного пальца
    if (js->active)
    {
        bool found = false;
        for (int i = 0; i < touchCount; i++)
        {
            if (GetTouchPointId(i) == js->touchId)
            {
                Vector2 p = GetTouchPosition(i);

                Vector2 diff = { p.x - js->basePos.x, p.y - js->basePos.y };
                float len = sqrtf(diff.x*diff.x + diff.y*diff.y);

                if (len > js->baseRadius) {
                    diff.x *= js->baseRadius / len;
                    diff.y *= js->baseRadius / len;
                    len = js->baseRadius;
                }

                js->knobPos = (Vector2){ js->basePos.x + diff.x, js->basePos.y + diff.y };

                if (len > 0.001f)
                    js->delta = (Vector2){ diff.x / js->baseRadius, diff.y / js->baseRadius };
                else
                    js->delta = (Vector2){ 0.0f, 0.0f };

                found = true;
                break;
            }
        }
        if (!found) {
            // Палец отпущен
            js->active  = false;
            js->touchId = -1;
            js->knobPos = js->basePos;
            js->delta   = (Vector2){ 0.0f, 0.0f };
        }
    }
}

static void DrawJoystick(const VirtualJoystick* js)
{
    DrawCircleV(js->basePos, js->baseRadius, (Color){ 255, 255, 255, 50 });
    DrawCircleLines((int)js->basePos.x, (int)js->basePos.y, js->baseRadius, (Color){ 255, 255, 255, 120 });
    DrawCircleV(js->knobPos, js->knobRadius, (Color){ 255, 255, 255, js->active ? 180 : 90 });
}

// ---------------------------------------------------------------------------
//  Зона свайпа (правая половина) — поворот камеры
// ---------------------------------------------------------------------------
struct LookArea {
    int     touchId;
    Vector2 lastPos;
    bool    active;
};

static void InitLookArea(LookArea* la)
{
    la->touchId = -1;
    la->lastPos = (Vector2){ 0, 0 };
    la->active  = false;
}

static Vector2 UpdateLookArea(LookArea* la, float screenW, float screenH)
{
    Vector2 delta = { 0, 0 };
    int touchCount = GetTouchPointCount();

    if (!la->active)
    {
        for (int i = 0; i < touchCount; i++)
        {
            Vector2 p = GetTouchPosition(i);
            if (p.x > screenW * 0.35f) // правая ~65% экрана
            {
                la->active  = true;
                la->touchId = GetTouchPointId(i);
                la->lastPos = p;
                break;
            }
        }
    }

    if (la->active)
    {
        bool found = false;
        for (int i = 0; i < touchCount; i++)
        {
            if (GetTouchPointId(i) == la->touchId)
            {
                Vector2 p = GetTouchPosition(i);
                delta.x = p.x - la->lastPos.x;
                delta.y = p.y - la->lastPos.y;
                la->lastPos = p;
                found = true;
                break;
            }
        }
        if (!found) {
            la->active  = false;
            la->touchId = -1;
        }
    }

    (void)screenH;
    return delta;
}

// ---------------------------------------------------------------------------
//  Отрисовка маяка
// ---------------------------------------------------------------------------
static void DrawLighthouse(Vector3 base, float radius, float height, Color bodyColor, Color stripeColor)
{
    const int segments = 6;
    float segH = height / segments;
    for (int i = 0; i < segments; i++)
    {
        Color c = (i % 2 == 0) ? bodyColor : stripeColor;
        float y0 = base.y + i * segH;
        DrawCylinder((Vector3){ base.x, y0, base.z },
                     radius * (1.0f - 0.05f * i),
                     radius * (1.0f - 0.05f * (i + 1)),
                     segH, 16, c);
    }
    Vector3 top = { base.x, base.y + height, base.z };
    DrawCylinder(top, radius * 0.9f, radius * 0.9f, 0.4f, 16, DARKGRAY);
    DrawCylinder((Vector3){ top.x, top.y + 0.4f, top.z },
                 radius * 0.7f, radius * 0.7f, 1.0f, 16, (Color){ 255, 240, 150, 220 });
    DrawCylinder((Vector3){ top.x, top.y + 1.4f, top.z },
                 0.0f, radius * 0.9f, 1.0f, 16, DARKGRAY);
    DrawSphere((Vector3){ top.x, top.y + 0.9f, top.z }, 0.25f, YELLOW);
}

// ---------------------------------------------------------------------------
//  Биллборд
// ---------------------------------------------------------------------------
static void DrawBillboard(Camera camera, Texture2D tex, Vector3 pos, float w, float h, Color tint)
{
    Vector3 toCam = Vector3Subtract(camera.position, pos);
    toCam.y = 0.0f;
    float len = Vector3Length(toCam);
    if (len < 0.0001f) return;
    toCam = Vector3Scale(toCam, 1.0f / len);

    Vector3 right = Vector3Normalize(Vector3CrossProduct((Vector3){ 0, 1, 0 }, toCam));
    Vector3 halfW = Vector3Scale(right, w * 0.5f);
    Vector3 up    = { 0, h, 0 };

    Vector3 bl = Vector3Subtract(pos, halfW);
    Vector3 br = Vector3Add(pos, halfW);
    Vector3 tr = Vector3Add(br, up);
    Vector3 tl = Vector3Add(bl, up);

    rlSetTexture(tex.id);
    rlBegin(RL_QUADS);
        rlColor4ub(tint.r, tint.g, tint.b, tint.a);
        rlNormal3f(toCam.x, 0.0f, toCam.z);
        rlTexCoord2f(0.0f, 1.0f); rlVertex3f(bl.x, bl.y, bl.z);
        rlTexCoord2f(1.0f, 1.0f); rlVertex3f(br.x, br.y, br.z);
        rlTexCoord2f(1.0f, 0.0f); rlVertex3f(tr.x, tr.y, tr.z);
        rlTexCoord2f(0.0f, 0.0f); rlVertex3f(tl.x, tl.y, tl.z);
    rlEnd();
    rlSetTexture(0);
}

// ---------------------------------------------------------------------------
//  Заглушка Jane Mik
// ---------------------------------------------------------------------------
static Texture2D MakePlaceholderJane()
{
    Image img = GenImageColor(128, 192, BLANK);
    ImageDrawRectangle(&img, 40, 10, 48, 48, (Color){ 240, 200, 160, 255 });
    ImageDrawRectangle(&img, 52, 24, 8,  8,  BLACK);
    ImageDrawRectangle(&img, 68, 24, 8,  8,  BLACK);
    ImageDrawRectangle(&img, 48, 80, 32, 70, (Color){ 60, 120, 200, 255 });
    ImageDrawRectangle(&img, 20, 80, 24, 60, (Color){ 240, 200, 160, 255 });
    ImageDrawRectangle(&img, 84, 80, 24, 60, (Color){ 240, 200, 160, 255 });
    ImageDrawRectangle(&img, 44, 150, 16, 40, (Color){ 40,  60, 100, 255 });
    ImageDrawRectangle(&img, 68, 150, 16, 40, (Color){ 40,  60, 100, 255 });
    Texture2D tex = LoadTextureFromImage(img);
    UnloadImage(img);
    return tex;
}

// ---------------------------------------------------------------------------
//  Главный вход
//  На Android Raylib собирается как native-библиотека (libmain.so),
//  и точкой входа для NativeActivity служит именно main().
// ---------------------------------------------------------------------------
int main(void)
{
    // --- Инициализация окна ---------------------------------------------------
    SetConfigFlags(FLAG_MSAA_4X_HINT | FLAG_WINDOW_RESIZABLE);
    InitWindow(0, 0, "Underwater");   // 0,0 = размер экрана устройства
    SetTargetFPS(60);

    int screenW = GetScreenWidth();
    int screenH = GetScreenHeight();

    // --- Камера ---------------------------------------------------------------
    Camera3D camera = { 0 };
    camera.position   = (Vector3){ 0.0f, 8.0f, 14.0f };
    camera.target     = (Vector3){ 0.0f, 1.0f,  0.0f };
    camera.up         = (Vector3){ 0.0f, 1.0f,  0.0f };
    camera.fovy       = 55.0f;
    camera.projection = CAMERA_PERSPECTIVE;

    // --- Игрок ----------------------------------------------------------------
    Vector3 playerPos = { 0.0f, 0.0f, 0.0f };
    float   playerYaw = 0.0f;
    float   camPitch  = 0.35f;   // наклон камеры вверх/вниз

    // --- Текстура Jane --------------------------------------------------------
    // На Android файл лежит внутри APK в assets/resources/jane.png.
    // Raylib корректно читает такие пути без префикса "/".
    Texture2D janeTex;
    if (FileExists("resources/jane.png")) {
        janeTex = LoadTexture("resources/jane.png");
    } else {
        janeTex = MakePlaceholderJane();
    }
    SetTextureFilter(janeTex, TEXTURE_FILTER_BILINEAR);

    // --- Маяк -----------------------------------------------------------------
    Vector3 lighthousePos = { 6.0f, 0.0f, -4.0f };

    // --- Декорации ------------------------------------------------------------
    const int decoCount = 14;
    Vector3 deco[decoCount];
    for (int i = 0; i < decoCount; i++)
    {
        float ang = (float)i * (2.0f * PI / decoCount);
        float r   = ISLAND_RADIUS * (0.4f + 0.35f * ((i * 37) % 100) / 100.0f);
        deco[i] = (Vector3){ cosf(ang) * r, 0.0f, sinf(ang) * r };
    }

    // --- Виртуальные контролы -------------------------------------------------
    float jsRadius = (float)GetScreenHeight() * 0.11f;
    VirtualJoystick joystick;
    InitJoystick(&joystick,
                 (Vector2){ jsRadius + 40.0f, (float)screenH - jsRadius - 40.0f },
                 jsRadius,
                 jsRadius * 0.45f);

    LookArea lookArea;
    InitLookArea(&lookArea);

    float waterTime = 0.0f;

    // =========================================================================
    //  Основной цикл
    // =========================================================================
    while (!WindowShouldClose())
    {
        screenW = GetScreenWidth();
        screenH = GetScreenHeight();

        // Пересчитываем позиции контролов при повороте экрана
        joystick.basePos    = (Vector2){ joystick.baseRadius + 40.0f,
                                         (float)screenH - joystick.baseRadius - 40.0f };
        joystick.baseRadius = (float)screenH * 0.11f;
        joystick.knobRadius = joystick.baseRadius * 0.45f;

        float dt = GetFrameTime();
        waterTime += dt;

        // --- Ввод -----------------------------------------------------------------
        UpdateJoystick(&joystick);
        Vector2 lookDelta = UpdateLookArea(&lookArea, (float)screenW, (float)screenH);

        // Вращение камеры
        playerYaw -= lookDelta.x * 0.006f;
        camPitch  += lookDelta.y * 0.004f;
        if (camPitch < 0.05f) camPitch = 0.05f;
        if (camPitch > 1.2f)  camPitch = 1.2f;

        // Движение относительно направления камеры
        Vector3 forward = { sinf(playerYaw), 0.0f, cosf(playerYaw) };
        Vector3 right   = { cosf(playerYaw), 0.0f, -sinf(playerYaw) };

        Vector3 move = { 0 };
        move = Vector3Add(move, Vector3Scale(forward, -joystick.delta.y));
        move = Vector3Add(move, Vector3Scale(right,    joystick.delta.x));

        if (Vector3Length(move) > 0.05f)
        {
            move = Vector3Normalize(move);
            Vector3 next = Vector3Add(playerPos, Vector3Scale(move, PLAYER_SPEED * dt));
            float dist = sqrtf(next.x*next.x + next.z*next.z);
            if (dist <= ISLAND_RADIUS - 0.5f)
            {
                playerPos.x = next.x;
                playerPos.z = next.z;
            }
        }

        // --- Камера следует за игроком -------------------------------------------
        float camDist = 12.0f;
        float camHeight = 8.0f * camPitch + 4.0f;
        camera.position = (Vector3){
            playerPos.x - sinf(playerYaw) * camDist,
            playerPos.y + camHeight,
            playerPos.z - cosf(playerYaw) * camDist
        };
        camera.target = (Vector3){ playerPos.x, playerPos.y + PLAYER_HEIGHT * 0.6f, playerPos.z };

        // =========================================================================
        //  DRAW
        // =========================================================================
        BeginDrawing();
        ClearBackground((Color){ 20, 40, 70, 255 });

        BeginMode3D(camera);
            // Море
            DrawPlane((Vector3){ 0.0f, -0.5f, 0.0f }, (Vector2){ 400.0f, 400.0f },
                      (Color){ 20, 70, 110, 255 });
            for (int i = 0; i < 6; i++)
            {
                float y = -0.4f + 0.05f * sinf(waterTime * 1.5f + i);
                DrawPlane((Vector3){ 0.0f, y, 0.0f }, (Vector2){ 380.0f, 380.0f },
                          (Color){ 30, 90, 140, 40 });
            }

            // Остров
            DrawCylinder((Vector3){ 0.0f, -1.2f, 0.0f },
                         ISLAND_RADIUS, ISLAND_RADIUS, 1.2f, 48,
                         (Color){ 210, 180, 130, 255 });
            DrawCylinder((Vector3){ 0.0f, -0.05f, 0.0f },
                         ISLAND_RADIUS - 0.2f, ISLAND_RADIUS - 0.2f, 0.15f, 48,
                         (Color){ 60, 130, 70, 255 });

            // Маяк
            DrawLighthouse(lighthousePos, 1.4f, 7.0f, (Color){ 200, 60, 60, 255 }, WHITE);

            // Декорации
            for (int i = 0; i < decoCount; i++)
            {
                float s = 0.4f + 0.4f * ((i * 13) % 10) / 10.0f;
                DrawCube((Vector3){ deco[i].x, s * 0.5f, deco[i].z }, s, s, s,
                         (Color){ 120, 110, 100, 255 });
            }

            // Jane Mik (биллборд)
            DrawBillboard(camera, janeTex, playerPos, SPRITE_WIDTH, SPRITE_HEIGHT, WHITE);

            // Тень
            DrawCylinder((Vector3){ playerPos.x, 0.01f, playerPos.z },
                         0.6f, 0.6f, 0.02f, 16, (Color){ 0, 0, 0, 100 });
        EndMode3D();

        // HUD
        DrawText("Underwater", 10, 10, 24, RAYWHITE);
        DrawText("Left: move  Right: look", 10, 38, 16, LIGHTGRAY);
        DrawFPS(10, (int)screenH - 30);

        DrawJoystick(&joystick);

        EndDrawing();
    }

    UnloadTexture(janeTex);
    CloseWindow();
    return 0;
}
