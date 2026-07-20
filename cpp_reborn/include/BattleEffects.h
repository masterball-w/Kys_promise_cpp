#pragma once

#include <string>

enum class BattleEffectType {
    None = 0,
    
    // Damage Modifiers
    DamageBoost_Female = 2,           // 女性加成所有伤害 (State 2)
    Drink_Double = 3,                 // 饮酒功效加倍 (State 3)
    
    // Defensive / Special
    Transfer_Damage = 4,              // 乾坤大挪移 (State 4)
    Reflect_Damage = 5,               // 斗转星移 (State 5)
    Ignore_Poison = 6,                // 百毒不侵 (State 6) (Logic in CalHurt/UsePoison)
    Attack_Poison = 7,                // 攻击带毒 (State 7)
    
    Dodge_Boost = 8,                  // 凌波微步 (State 8)
    Damage_Random = 9,                // 伤害波动 (State 9)
    Save_MP = 10,                     // 节省内力 (State 10)
    Regen_HP = 11,                    // 每回合恢复生命 (State 11)
    Ignore_Debuff = 12,               // 负面状态免疫 (State 12)
    
    DamageBoost_All = 13,             // 加成所有伤害 (State 13)
    
    // 14: 随机二次攻击; 15-18: 拳/剑/刀/奇门威力加成 (≈14+MagicType)
    Attack_Twice_Random = 14,
    
    Absorb_HP = 21,                   // 吸血 (State 21)
    Attack_Range_Boost = 22,          // 增加攻击距离 (State 22)
    Regen_MP = 23,                    // 每回合恢复内力 (State 23)
    HiddenWeapon_Boost = 24,          // 暗器距离增加 (State 24)
    Poison_Attack_Boost = 25,         // 附加杀伤吸收内力 (State 25)

    // New gongti / equip states (continue numbering)
    Stack_Attack = 26,                // 每回合提高攻击（最多10次）
    Aura_Poison = 27,                 // 令附近敌人中毒（行动结束时 7x7）
    Boost_Med_Detox = 28,             // 大幅提升医疗和解毒的效果（+50% 且范围+2）
};

/** Display name for BattleState / Item.BattleEffect (1-based IDs). Empty if unknown. */
inline std::string GetBattleEffectDisplayName(int stateId) {
    switch (stateId) {
        case 1: return "体力不减";
        case 2: return "女性武功威力加成";
        case 3: return "饮酒功效加倍";
        case 4: return "随机伤害转移";
        case 5: return "随机伤害反噬";
        case 6: return "内伤免疫";
        case 7: return "杀伤体力";
        case 8: return "增加闪躲几率";
        case 9: return "攻击力随等级循环增减";
        case 10: return "内力消耗减少";
        case 11: return "每回合恢复生命";
        case 12: return "负面状态免疫";
        case 13: return "全部武功威力加成";
        case 14: return "随机二次攻击";
        case 15: return "拳掌武功威力加成";
        case 16: return "剑术武功威力加成";
        case 17: return "刀法武功威力加成";
        case 18: return "奇门武功威力加成";
        case 19: return "增加内伤几率";
        case 20: return "增加封穴几率";
        case 21: return "攻击微量吸血";
        case 22: return "攻击距离增加";
        case 23: return "每回合恢复内力";
        case 24: return "使用暗器距离增加";
        case 25: return "附加杀伤吸收内力";
        case 26: return "每回合提高攻击";
        case 27: return "令附近敌人中毒";
        case 28: return "大幅提升医疗和解毒的效果";
        default: return {};
    }
}
