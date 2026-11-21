#pragma once

#include "../units/Unit.hpp"
#include "../config/SimConfig.hpp"
#include <cstdint>
#include <string>
#include <vector>

namespace sim {

/**
 * @brief Đường đạn / tia bắn hiển thị trên chiến trường
 */
struct Tracer {
    int32_t     startPosX{0};  // Vị trí bắn (mili-ô)
    int32_t     startPosY{0};
    int32_t     endPosX{0};    // Vị trí mục tiêu (mili-ô)
    int32_t     endPosY{0};
    uint64_t    spawnTick{0};  // Thời điểm bắn (tick)
    std::string ammoType{"AP"};
};

/**
 * @brief Kết quả trận đấu
 */
enum class MatchResult : uint8_t {
    InProgress = 0,
    Victory = 1,  // Phe Player thắng
    Defeat = 2,   // Phe Player thua
    Draw = 3      // Hòa
};

/**
 * @brief Lý do / Loại hình kết thúc ván đấu
 */
enum class MatchEndKind {
    CommanderKilled = 0, // Tư lệnh bị tiêu diệt
    TroopsWiped,         // Quét sạch toàn bộ quân lực
    Timeout,             // Hết thời gian trận đấu
    Draw                 // Hòa trận
};

/**
 * @brief Thông tin chi tiết khi ván kết thúc
 */
struct MatchEndInfo {
    MatchResult  result{MatchResult::InProgress};
    std::string  reason;
    MatchEndKind kind{MatchEndKind::Draw};
};

/**
 * @brief Hệ thống tính toán chiến đấu, sát thương fixed-point và quy tắc bắn
 */
class CombatSystem {
public:
    // Các hàm nội suy tuyến tính (tỉ lệ 1000)
    static int64_t getHeBonus(int targetArmor);
    static int64_t getHeArmorFactor(int targetArmor);
    static int64_t getApBonus(int targetArmor, bool isSmallArms);
    static int64_t getSmallArmsFactor(int targetArmor);
    static int64_t getApArmorFactor(int targetArmor);
    static int64_t getAntiAirFactor(UnitType attackerType, UnitDomain targetDomain);

    /**
     * @brief Tính sát thương một đợt bắn (Volley) bằng số nguyên int64 fixed-point (x1000).
     * @param attackerType Loại đơn vị tấn công
     * @param weapon Cấu hình vũ khí
     * @param aliveUnits Số quân còn sống trong chấm tấn công
     * @param targetArmor Chỉ số giáp của mục tiêu bị trúng
     * @param targetDomain Miền của mục tiêu (Land / Air / Sea)
     * @param isSplash True nếu đây là sát thương nổ lan cho chấm bên cạnh
     * @param hitsOverride Ghi đè số phát trúng (mặc định -1: tính theo n_alive * rounds * accuracy)
     * @param splashRatio Tỉ lệ nổ lan (mặc định 550 tương ứng 55%)
     */
    static int64_t calculateVolleyDamage(
        UnitType attackerType,
        const WeaponConfig& weapon,
        int aliveUnits,
        int targetArmor,
        UnitDomain targetDomain,
        bool isSplash = false,
        int hitsOverride = -1,
        int64_t splashRatio = 550
    );

    /**
     * @brief Tự động chọn chỉ mục vũ khí cho tổng sát thương lớn nhất lên mục tiêu
     */
    static size_t chooseBestWeaponIndex(
        UnitType attackerType,
        const std::vector<WeaponConfig>& weapons,
        int aliveUnits,
        int targetArmor,
        UnitDomain targetDomain
    );

    /**
     * @brief Kiểm tra đơn vị có thể tấn công mục tiêu không (theo khoảng cách, phe, và canTargetAir/Ground/Sea)
     */
    static bool canAttackTarget(const Unit& attacker, const Unit& target);
};

} // namespace sim
