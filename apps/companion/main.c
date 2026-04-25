#include "attune/attune_snapshot.h"

#include "raylib.h"

#define RAYGUI_IMPLEMENTATION
#include "raygui.h"

#include <stdio.h>
#include <string.h>

typedef enum CompanionTab {
    COMPANION_TAB_ATTUNES = 0,
    COMPANION_TAB_ADDONS = 1,
    COMPANION_TAB_SETTINGS = 2
} CompanionTab;

typedef struct CompanionState {
    CompanionTab tab;
    char synastria_path[512];
    char snapshot_path[768];
    char status[256];
    AhcDailyAttuneSnapshot snapshot;
} CompanionState;

static void draw_header(void)
{
    DrawText("Attune Helper Companion", 24, 20, 28, RAYWHITE);
    DrawText("Synastria addon manager and daily attune tracker", 26, 54, 16, LIGHTGRAY);
}

static void draw_tabs(CompanionState *state)
{
    const Rectangle tabs = { 24.0f, 88.0f, 380.0f, 32.0f };
    state->tab = (CompanionTab)GuiToggleGroup(tabs, "Attunes;Addons;Settings", (int)state->tab);
}

static void draw_snapshot_card(const CompanionState *state)
{
    const int x = 32;
    int y = 142;
    char line[128];

    GuiGroupBox((Rectangle){ 24.0f, 130.0f, 380.0f, 210.0f }, "Latest Daily Snapshot");

    if (!state->snapshot.found) {
        DrawText("No AttuneHelper snapshot loaded yet.", x, y, 18, RAYWHITE);
        DrawText("Set a SavedVariables file path in Settings.", x, y + 28, 16, LIGHTGRAY);
        return;
    }

    snprintf(line, sizeof(line), "Date: %s", state->snapshot.date[0] ? state->snapshot.date : "unknown");
    DrawText(line, x, y, 18, RAYWHITE);
    y += 30;

    snprintf(line, sizeof(line), "Account: %d", state->snapshot.account);
    DrawText(line, x, y, 18, RAYWHITE);
    y += 38;

    snprintf(line, sizeof(line), "Warforged: %d", state->snapshot.warforged);
    DrawText(line, x, y, 18, SKYBLUE);
    y += 28;

    snprintf(line, sizeof(line), "Lightforged: %d", state->snapshot.lightforged);
    DrawText(line, x, y, 18, GOLD);
    y += 28;

    snprintf(line, sizeof(line), "Titanforged: %d", state->snapshot.titanforged);
    DrawText(line, x, y, 18, GREEN);
}

static void draw_graph_placeholder(const CompanionState *state)
{
    const Rectangle bounds = { 430.0f, 130.0f, 430.0f, 300.0f };
    GuiGroupBox(bounds, "Daily Attune History");

    DrawLine(466, 380, 830, 380, GRAY);
    DrawLine(466, 170, 466, 380, GRAY);

    if (!state->snapshot.found) {
        DrawText("History graph will populate after snapshots are saved locally.", 462, 250, 16, LIGHTGRAY);
        return;
    }

    const int base_y = 380;
    const int scale = 12;
    const int war_height = state->snapshot.warforged / scale;
    const int light_height = state->snapshot.lightforged / scale;
    const int titan_height = state->snapshot.titanforged / scale;

    DrawRectangle(520, base_y - war_height, 42, war_height, SKYBLUE);
    DrawRectangle(604, base_y - light_height, 42, light_height, GOLD);
    DrawRectangle(688, base_y - titan_height, 42, titan_height, GREEN);

    DrawText("War", 524, 392, 14, LIGHTGRAY);
    DrawText("Light", 606, 392, 14, LIGHTGRAY);
    DrawText("Titan", 690, 392, 14, LIGHTGRAY);
}

static void draw_attunes_tab(const CompanionState *state)
{
    draw_snapshot_card(state);
    draw_graph_placeholder(state);
}

static void draw_addons_tab(void)
{
    GuiGroupBox((Rectangle){ 24.0f, 130.0f, 836.0f, 300.0f }, "Addon Manager");
    DrawText("Addon manifest, install, update, and uninstall controls will go here.", 44, 166, 18, RAYWHITE);
    DrawText("Scope is addon packages only. No client downloads, accounts, or WTF cloud storage.", 44, 198, 16, LIGHTGRAY);
}

static void draw_settings_tab(CompanionState *state)
{
    GuiGroupBox((Rectangle){ 24.0f, 130.0f, 836.0f, 300.0f }, "Settings");

    DrawText("Synastria folder", 44, 166, 16, LIGHTGRAY);
    GuiTextBox((Rectangle){ 44.0f, 190.0f, 760.0f, 30.0f }, state->synastria_path, (int)sizeof(state->synastria_path), true);

    DrawText("AttuneHelper.lua path", 44, 236, 16, LIGHTGRAY);
    GuiTextBox((Rectangle){ 44.0f, 260.0f, 760.0f, 30.0f }, state->snapshot_path, (int)sizeof(state->snapshot_path), true);

    if (GuiButton((Rectangle){ 44.0f, 312.0f, 180.0f, 34.0f }, "Load Snapshot")) {
        if (ahc_parse_daily_attune_snapshot_file(state->snapshot_path, &state->snapshot)) {
            snprintf(state->status, sizeof(state->status), "Loaded snapshot for %s.", state->snapshot.date);
        } else {
            snprintf(state->status, sizeof(state->status), "Could not load DailyAttuneSnapshot.");
        }
    }
}

int main(void)
{
    CompanionState state;
    memset(&state, 0, sizeof(state));
    state.tab = COMPANION_TAB_ATTUNES;
    snprintf(state.status, sizeof(state.status), "Ready.");

    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    InitWindow(900, 520, "Attune Helper Companion");
    SetTargetFPS(60);
    GuiSetStyle(DEFAULT, TEXT_SIZE, 16);

    while (!WindowShouldClose()) {
        BeginDrawing();
        ClearBackground((Color){ 24, 26, 32, 255 });

        draw_header();
        draw_tabs(&state);

        switch (state.tab) {
            case COMPANION_TAB_ATTUNES:
                draw_attunes_tab(&state);
                break;
            case COMPANION_TAB_ADDONS:
                draw_addons_tab();
                break;
            case COMPANION_TAB_SETTINGS:
                draw_settings_tab(&state);
                break;
        }

        DrawText(state.status, 24, 474, 16, LIGHTGRAY);

        EndDrawing();
    }

    CloseWindow();
    return 0;
}
