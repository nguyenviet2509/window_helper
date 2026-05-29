#pragma once
// Action priority ladder. Lower value = more urgent (matches InputCmd::priority).
enum Priority : int {
    P0_HpEmergency = 0,
    P1_MpSp        = 1,
    P2_Recall      = 2,
    P3_Combat      = 3,
    P4_Buff        = 4,
};
