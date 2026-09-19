#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <assert.h>
#include <string.h>

#include "sim.h"

static bool s_zone_queried = false;
static void test_show_zone_status(char *str, char *s0, char *s1, char *s2, char *s3, char *s4, int x, int y)
{
    (void)str; (void)s0; (void)s1; (void)s2; (void)s3; (void)s4; (void)x; (void)y;
    s_zone_queried = true;
}

int main(void)
{
    printf("--- Running Micropolis Headless Sanity Tests ---\n");

    /* 1. Initialization */
    initMapArrays();
    sim = MakeNewSim();
    assert(sim != NULL);
    sim->editor = MakeNewView();
    assert(sim->editor != NULL);
    sim->editor->w_width = 400;
    sim->editor->w_height = 240;
    sim->editor->tool_state = roadState;

    InitWillStuff();
    InitGame();

    g_sim_ui_callbacks.on_show_zone_status = test_show_zone_status;

    printf("[PASS] Core data structures and simulation instance initialized\n");

    /* 2. Embedded City Loading */
    bool loaded = LoadEmbeddedCity("about.cty");
    assert(loaded);
    printf("[PASS] Loaded embedded city 'about.cty' (City: %s, Funds: $%d, Time: %d)\n",
           CityName ? CityName : "Unknown", (int)TotalFunds, (int)CityTime);

    /* 3. Tool Placement */
    int tx = 10, ty = 10;
    for (int y = 5; y < 90; y++) {
        for (int x = 5; x < 110; x++) {
            if ((Map[x][y] & LOMASK) == DIRT) {
                tx = x; ty = y;
                goto found_dirt;
            }
        }
    }
found_dirt:
    QUAD funds_before = TotalFunds;
    int res = DoTool(sim->editor, roadState, tx, ty);
    assert(res == 1);
    assert(TotalFunds < funds_before); /* Spent $10 */
    short tile = Map[tx][ty] & LOMASK;
    assert(tile >= ROADBASE && tile <= LASTROAD);
    printf("[PASS] Road tool placement successfully placed tile %d at (%d,%d) and debited funds\n", tile, tx, ty);

    /* 4. Query Tool */
    s_zone_queried = false;
    DoTool(sim->editor, queryState, 10, 10);
    assert(s_zone_queried);
    printf("[PASS] Query tool callback fired with zone status\n");

    /* 5. Simulation Processing (50 frames) */
    SimSpeed = 3;
    for (int i = 0; i < 50; i++) {
        SimFrame();
        MoveObjects();
        if (i % 10 == 0) {
            animateTiles();
        }
    }
    printf("[PASS] 50 simulation frames completed successfully\n");

    /* 6. City Evaluation */
    CityEvaluation();
    assert(CityScore >= 0 && CityScore <= 1000);
    printf("[PASS] City evaluation computed: Score = %d/1000, Approval = %d%% Yes / %d%% No\n",
           CityScore, CityYes, CityNo);

    /* 7. Scenarios */
    LoadScenario(2); /* San Francisco 1906 */
    assert(ScenarioID == 2);
    printf("[PASS] Scenario 2 loaded: %s (Funds: $%d)\n", CityName ? CityName : "San Francisco", (int)TotalFunds);

    /* 8. Disasters */
    SetFire();
    MakeFlood();
    MakeEarthquake();
    MakeTornado();
    MakeMonster();
    MakeMeltdown();
    printf("[PASS] All disaster triggers executed safely\n");

    /* 9. Procedural Generation */
    GenerateNewCity();
    assert(TotalFunds > 0);
    printf("[PASS] Procedural terrain generator generated new map\n");

    printf("\n>>> ALL MICROPOLIS SIMULATION TESTS PASSED! <<<\n");
    return 0;
}
