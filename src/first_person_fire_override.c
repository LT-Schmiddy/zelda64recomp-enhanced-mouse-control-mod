#include "modding.h"
#include "recomputils.h"
#include "recompconfig.h"

#include "zelda64_mouse.h"

#include "global.h"
#include "functions.h"

#define CHECK_ITEM_IS_BOW(item) ((item == ITEM_BOW) || ((item >= ITEM_BOW_FIRE) && (item <= ITEM_BOW_LIGHT)))
#define CHECK_ITEM_IS_HOOKSHOT(item) (item == ITEM_HOOKSHOT)

typedef struct {
    bool cur;
    bool prev;
    bool press;
    bool rel;
} BtnState;
BtnState btnStateB;

typedef struct {
    u32 cur;
    u32 prev;
    u32 press;
    u32 rel;
} MouseBtnState;
MouseBtnState mouseBtnState;


typedef enum {
    OFF = 0,
    FIRST_PERSON = 1,
    AIMING = 2,
    HOLDING = 3
} RFiringBehavior;

// order matches EquipSlot enum
static u16 slot_to_btn_id[4] = {
    BTN_B,
    BTN_CLEFT,
    BTN_CDOWN,
    BTN_CRIGHT
};

static Input* sInput = NULL;

s32 Player_UpperAction_7(Player* thisx, PlayState* play);
s32 Player_UpperAction_8(Player* thisx, PlayState* play);
void Player_Action_43(Player* this, PlayState* play); // Free Look, Hookshot, Bow, etc.
void Player_Action_52(Player* this, PlayState* play); //Riding Epona
void Player_Action_81(Player* this, PlayState* play); // Shooting Gallery, Cremia's Milk Run

bool Player_isHoldingBow(Player* this) {
    return (
        this->heldItemAction == PLAYER_IA_BOW
        || this->heldItemAction == PLAYER_IA_BOW_FIRE
        || this->heldItemAction == PLAYER_IA_BOW_ICE
        || this->heldItemAction == PLAYER_IA_BOW_LIGHT
        || this->actionFunc == Player_Action_81 // For Bow Mini-Games
        );
}

bool Player_IsAimingBow(Player* this, PlayState* play) {
    return (Player_isHoldingBow(this)) &&
        (
            this->upperActionFunc == Player_UpperAction_8
            || this->upperActionFunc == Player_UpperAction_7
        );
}

bool Player_IsAimingHookshot(Player* this, PlayState* play) {
    return (Player_IsHoldingHookshot(this)) &&
        (
            this->upperActionFunc == Player_UpperAction_8
            || this->upperActionFunc == Player_UpperAction_7
        );
}

bool Player_IsFirstPersonBow(Player* this, PlayState* play) {
    return (Player_isHoldingBow(this)) &&
        (
            this->actionFunc == Player_Action_43
            || this->actionFunc == Player_Action_52
            || this->actionFunc == Player_Action_81
        );
}

bool Player_IsFirstPersonHookshot(Player* this, PlayState* play) {
    return (Player_IsHoldingHookshot(this)) &&
        (
            this->actionFunc == Player_Action_43
            || this->actionFunc == Player_Action_52
            || this->actionFunc == Player_Action_81
        );
}

bool ShouldAllowRFiring(Player* this, PlayState* play) {
    RFiringBehavior bow_r = recomp_get_config_u32("bow-fire-with-r");
    RFiringBehavior hookshot_r = recomp_get_config_u32("hookshot-fire-with-r");
    return (
        (bow_r == FIRST_PERSON && Player_IsFirstPersonBow(this, play))
        || (bow_r == AIMING && Player_IsAimingBow(this, play))
        || (bow_r == HOLDING && Player_isHoldingBow(this))
        || (hookshot_r == FIRST_PERSON && Player_IsFirstPersonHookshot(this, play))
        || (hookshot_r == AIMING && Player_IsAimingHookshot(this, play))
        || (hookshot_r == HOLDING && Player_IsHoldingHookshot(this))
    );
}


bool ShouldAllowBFiring(Player* this, PlayState* play) {
    RFiringBehavior bow_b = recomp_get_config_u32("bow-fire-with-b");
    RFiringBehavior hookshot_b = recomp_get_config_u32("hookshot-fire-with-b");
    return (
        (bow_b == FIRST_PERSON && Player_IsFirstPersonBow(this, play))
        || (bow_b == AIMING && Player_IsAimingBow(this, play))
        || (bow_b == HOLDING && Player_isHoldingBow(this))
        || (hookshot_b == FIRST_PERSON && Player_IsFirstPersonHookshot(this, play))
        || (hookshot_b == AIMING && Player_IsAimingHookshot(this, play))
        || (hookshot_b == HOLDING && Player_IsHoldingHookshot(this))
    );
}
void BtnState_Record( Input* input, BtnState* state, u16 btn, bool should_mask) {
    state->cur = input->cur.button & btn;
    state->prev = input->prev.button & btn;
    state->press = input->press.button & btn;
    state->rel = input->rel.button & btn;

    if (should_mask) {
        input->cur.button &= ~btn;
        input->prev.button &= ~btn;
        input->press.button &= ~btn;
        input->rel.button &= ~btn;
    }
}

void MouseState_Update(MouseBtnState* m) {
    u32 new_cur = zelda64_get_mouse_buttons();
    
    // prev is the cur from last update.
    m->prev = m->cur;

    // If held previously, but not now, it was released.
    m->rel = m->prev & ~new_cur;

    // If held now but now prev, pressed.
    m->press = new_cur & ~m->prev;

    // update current:
    m->cur = new_cur;
}

RECOMP_HOOK("Player_Update") void pre_Player_UpdateCommon(Player* this, PlayState* play) {
    MouseState_Update(&mouseBtnState);
    recomp_printf("mouseBtnState: cur=%i, press=%i, prev=%i, rel=%i\n", mouseBtnState.cur, mouseBtnState.press, mouseBtnState.prev, mouseBtnState.rel);

    Input* input = CONTROLLER1(&play->state);
    // Kafei Prevention:
    if (this->actor.id != ACTOR_PLAYER) {
        return;
    }
    // recomp_printf("player->state1 = %08X", this->stateFlags1);
    // Checking if we need to do anything.
    if (ShouldAllowBFiring(this, play)) { 
        BtnState_Record(input, &btnStateB, BTN_B, true);
        EquipSlot spoof_button = EQUIP_SLOT_NONE;
        for (EquipSlot i = EQUIP_SLOT_B; i <= EQUIP_SLOT_C_RIGHT; i++) {
            u8 equippedItem = gSaveContext.save.saveInfo.equips.buttonItems[0][i];
            if ((Player_isHoldingBow(this) && CHECK_ITEM_IS_BOW(equippedItem)) 
                || (Player_IsHoldingHookshot(this) && CHECK_ITEM_IS_HOOKSHOT(equippedItem))) {
                spoof_button = i;
                break;
            }
        }

        if (btnStateB.press) {
            input->press.button |= slot_to_btn_id[spoof_button];
        }
        if (btnStateB.cur) {
            input->cur.button |= slot_to_btn_id[spoof_button];
        }
        if (btnStateB.prev) {
            input->prev.button |= slot_to_btn_id[spoof_button];
        }
        if (btnStateB.rel) {
            input->rel.button |= slot_to_btn_id[spoof_button];
        }

    }
}
