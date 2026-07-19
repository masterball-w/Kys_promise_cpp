#pragma once

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
    Attack_Twice = 11,                // 左右互搏 (State 11) - Check usage, sometimes 11 or 14?
    Ignore_Debuff = 12,               // 免疫异常? (State 12 - No Poison/frozen?)
    
    DamageBoost_All = 13,             // 加成所有伤害 (State 13)
    
    // 14 + MagicType: Specific Boost
    // 15 + MagicType: Specific Boost (HurtType 1?)
    
    Attack_Twice_Random = 14,         // 左右互搏 (Random check) - Wait, confirm usage
    
    Absorb_HP = 21,                   // 吸血 (State 21)
    Attack_Range_Boost = 22,          // 增加攻击距离 (State 22)
    Attack_All_Range = 23,            // 全屏攻击? (State 23)
    HiddenWeapon_Boost = 24,          // 霹雳心法 (State 24)
    Poison_Attack_Boost = 25,         // 毒攻增强 (State 25)
};
