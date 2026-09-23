#pragma once
/*
 * LonelyBinaryGFX - the drawing layer shared by every Lonely Binary display.
 *
 * You normally do not include this directly. Include your device library -
 * LonelyBinaryVGA.h, LonelyBinaryDisplay.h, LonelyBinaryEPaper.h - and it
 * brings this with it, already wired to the panel you have.
 *
 * Include it directly only when you are writing code that takes a display of
 * any kind:
 *
 *     void drawGauge(LB_Canvas &c, float value);   // works on TFT, VGA, e-paper
 *
 * That function is the reason this library exists. Before it, the product line
 * spoke three different graphics APIs and no such function could be written.
 *
 * The contract is canvas.yaml in this repository. It is frozen at version 1 and
 * carries the reasoning behind every decision, including the ones that were
 * decided against.
 */
#include "LB_Color.h"
#include "LB_Panel.h"
#include "LB_Canvas.h"
#include "LB_MemPanel.h"
