#pragma once

#include "Localisation/StringIds.h"
#include "Object.h"
#include "Speed.hpp"
#include <OpenLoco/Core/EnumFlags.hpp>
#include <span>

namespace OpenLoco
{
    namespace ObjectManager
    {
        struct DependentObjects;
    }
    namespace Gfx
    {
        class DrawingContext;
    }

    enum class TransportMode : uint8_t
    {
        rail = 0,
        road = 1,
        air = 2,
        water = 3
    };

    enum class SimpleAnimationType : uint8_t
    {
        none = 0,
        steam_puff1,
        steam_puff2,
        steam_puff3,
        diesel_exhaust1,
        electric_spark1,
        electric_spark2,
        diesel_exhaust2,
        ship_wake
    };

    namespace SpriteIndex
    {
        constexpr uint8_t null = 0xFF;
        constexpr uint8_t isReversed = (1 << 7); // the bogie or body should be drawn reversed
    }

#pragma pack(push, 1)
    struct VehicleObjectFrictionSound
    {
        uint8_t soundObjectId;     // 0x0
        Speed32 minSpeed;          // 0x1 below this speed no sound created
        uint8_t speedFreqFactor;   // 0x5
        uint16_t baseFrequency;    // 0x6
        uint8_t speedVolumeFactor; // 0x8
        uint8_t baseVolume;        // 0x9
        uint8_t maxVolume;         // 0xA
    };
    static_assert(sizeof(VehicleObjectFrictionSound) == 0xB);

    struct VehicleSimpleMotorSound
    {
        uint8_t soundObjectId;      // 0x0
        uint16_t idleFrequency;     // 0x1
        uint8_t idleVolume;         // 0x3
        uint16_t coastingFrequency; // 0x4
        uint8_t coastingVolume;     // 0x6
        uint16_t accelerationBaseFreq;
        uint8_t acclerationVolume;
        uint16_t freqIncreaseStep;  // 0xA
        uint16_t freqDecreaseStep;  // 0xC
        uint8_t volumeIncreaseStep; // 0xE
        uint8_t volumeDecreaseStep; // 0xF
        uint8_t speedFreqFactor;    // bit-shift right of vehicle speed, added to calculated base frequency
    };
    static_assert(sizeof(VehicleSimpleMotorSound) == 0x11);

    struct VehicleGearboxMotorSound
    {
        uint8_t soundObjectId;         // 0x0
        uint16_t idleFrequency;        // 0x1
        uint8_t idleVolume;            // 0x2
        uint16_t firstGearFrequency;   // 0x4 All subsequent gears are based on this frequency
        Speed16 firstGearSpeed;        // 0x6
        uint16_t secondGearFreqOffset; // 0x8
        Speed16 secondGearSpeed;       // 0xA
        uint16_t thirdGearFreqOffset;  // 0xC
        Speed16 thirdGearSpeed;        // 0xE
        uint16_t fourthGearFreqOffset; // 0x10
        uint8_t coastingVolume;
        uint8_t acceleratingVolume;
        uint16_t freqIncreaseStep;  // 0x14
        uint16_t freqDecreaseStep;  // 0x16
        uint8_t volumeIncreaseStep; // 0x18
        uint8_t volumeDecreaseStep; // 0x19
        uint8_t speedFreqFactor;    // bit-shift right of vehicle speed, added to calculated base frequency
    };
    static_assert(sizeof(VehicleGearboxMotorSound) == 0x1B);

    struct VehicleObjectSimpleAnimation
    {
        uint8_t objectId;           // 0x00 (object loader fills this in)
        uint8_t emitterVerticalPos; // 0x01
        SimpleAnimationType type;   // 0x02
    };
    static_assert(sizeof(VehicleObjectSimpleAnimation) == 0x3);

    struct VehicleObjectCar
    {
        uint8_t frontBogiePosition;  // 0x00 distance from front of car component (not strictly body) to bogie pivot point
        uint8_t backBogiePosition;   // 0x01 distance from back of car component (not strictly body) to bogie pivot point
        uint8_t frontBogieSpriteInd; // 0x02 index of bogieSprites struct
        uint8_t backBogieSpriteInd;  // 0x03 index of bogieSprites struct
        uint8_t bodySpriteInd;       // 0x04 index of a bodySprites struct
        uint8_t emitterHorizontalPos;
    };
    static_assert(sizeof(VehicleObjectCar) == 0x6);

    enum class BogieSpriteFlags : uint8_t
    {
        none = 0U,
        hasSprites = 1U << 0,         // If not set then no bogie will be loaded
        rotationalSymmetry = 1U << 1, // requires half the number of sprites i.e. 16 instead of 32
        hasGentleSprites = 1U << 2,   // for gentle slopes
        hasSteepSprites = 1U << 3,    // for steep slopes
        largerBoundingBox = 1U << 4,  // Increases bounding box size
    };
    OPENLOCO_ENABLE_ENUM_OPERATORS(BogieSpriteFlags);

    struct VehicleObjectBogieSprite
    {
        uint8_t numAnimationFrames;   // 0x0 valid values 1, 2, 4 related to bogie->animationIndex (identical in value to numFramesPerRotation)
        BogieSpriteFlags flags;       // 0x1 BogieSpriteFlags
        uint8_t width;                // 0x2 sprite width
        uint8_t heightNegative;       // 0x3 sprite height negative
        uint8_t heightPositive;       // 0x4 sprite height positive
        uint8_t numFramesPerRotation; // 0x5
        uint32_t flatImageIds;        // 0x6 flat sprites
        uint32_t gentleImageIds;      // 0xA gentle sprites
        uint32_t steepImageIds;       // 0xE steep sprites

        constexpr bool hasFlags(BogieSpriteFlags flagsToTest) const
        {
            return (flags & flagsToTest) != BogieSpriteFlags::none;
        }
    };
    static_assert(sizeof(VehicleObjectBogieSprite) == 0x12);

    enum class BodySpriteFlags : uint8_t
    {
        none = 0U,
        hasSprites = 1U << 0,         // If not set then no body will be loaded
        rotationalSymmetry = 1U << 1, // requires half the number of sprites i.e. 32 instead of 64
        flag02Deprecated = 1U << 2,   // incomplete feature of vanilla. Do not repurpose until new object format
        hasGentleSprites = 1U << 3,   // for gentle slopes
        hasSteepSprites = 1U << 4,    // for steep slopes
        hasBrakingLights = 1U << 5,
        hasSpeedAnimation = 1U << 6, // Speed based animation (such as hydrofoil)
    };
    OPENLOCO_ENABLE_ENUM_OPERATORS(BodySpriteFlags);

    struct VehicleObjectBodySprite
    {
        uint8_t numFlatRotationFrames;   // 0x00 4, 8, 16, 32, 64?
        uint8_t numSlopedRotationFrames; // 0x01 4, 8, 16, 32?
        uint8_t numAnimationFrames;      // 0x02
        uint8_t numCargoLoadFrames;      // 0x03
        uint8_t numCargoFrames;          // 0x04
        uint8_t numRollFrames;           // 0x05 currently the only valid values are 1 and 3
        uint8_t halfLength;              // 0x06 the distance from pivot of body to one end of car component (not strictly the visible body, see CE68 locomotive)
        BodySpriteFlags flags;           // 0x07
        uint8_t width;                   // 0x08 sprite width
        uint8_t heightNegative;          // 0x09 sprite height negative
        uint8_t heightPositive;          // 0x0A sprite height positive
        uint8_t flatYawAccuracy;         // 0x0B 0 - 4 accuracy of yaw on flat built from numFlatRotationFrames (0 = lowest accuracy 3bits, 4 = highest accuracy 7bits)
        uint8_t slopedYawAccuracy;       // 0x0C 0 - 3 accuracy of yaw on slopes built from numSlopedRotationFrames  (0 = lowest accuracy 3bits, 3 = highest accuracy 6bits)
        uint8_t numFramesPerRotation;    // 0x0D numAnimationFrames * numCargoFrames * numRollFrames + 1 (for braking lights)
        uint32_t flatImageId;            // 0x0E
        uint32_t unkImageId;             // 0x12
        uint32_t gentleImageId;          // 0x16
        uint32_t steepImageId;           // 0x1A

        constexpr bool hasFlags(BodySpriteFlags flagsToTest) const
        {
            return (flags & flagsToTest) != BodySpriteFlags::none;
        }
    };

    enum class VehicleObjectFlags : uint16_t
    {
        // See github issue https://github.com/OpenLoco/OpenLoco/issues/2877 for discussion on unnamed flags
        none = 0U,
        alternatingDirection = 1U << 0, // sequential vehicles face alternating directions
        topAndTailPosition = 1U << 1,   // vehicle is forced to the rear of the train
        jacobsBogieFront = 1U << 2,
        jacobsBogieRear = 1U << 3,
        flag_04 = 1U << 4,
        centerPosition = 1U << 5, // vehicle is forced to the middle of train
        rackRail = 1U << 6,
        // Alternates between sprite 0 and sprite 1 for each vehicle of this type in a train
        // NOTE: This is for vehicles and not vehicle components (which can also do similar)
        alternatingCarSprite = 1U << 7,
        flag_08 = 1U << 8,
        aircraftIsTaildragger = 1U << 8,
        anyRoadType = 1U << 9, // set on all road vehicles except trams
        flag_10 = 1U << 10,
        cannotCoupleToSelf = 1U << 11,
        aircraftFlaresLanding = 1U << 11, // set only on Concorde
        mustHavePair = 1U << 12,          // train requires two or more of this vehicle
        canWheelslip = 1U << 13,          // set on all steam locomotives
        aircraftIsHelicopter = 1U << 13,
        refittable = 1U << 14,
        quietInvention = 1U << 15, // no newspaper announcement
    };
    OPENLOCO_ENABLE_ENUM_OPERATORS(VehicleObjectFlags);

    enum class DrivingSoundType : uint8_t
    {
        none,
        friction,
        simpleMotor,
        gearboxMotor
    };

    namespace NumStartSounds
    {
        constexpr uint8_t kHasCrossingWhistle = 1 << 7;
        constexpr uint8_t kMask = 0x7F;
    }

    struct VehicleObject
    {
        static constexpr auto kObjectType = ObjectType::vehicle;
        static constexpr auto kMaxBodySprites = 4;
        static constexpr auto kMaxCarComponents = 4;
        static constexpr auto kMaxStartSounds = 3;

        StringId name;      // 0x00
        TransportMode mode; // 0x02
        VehicleType type;   // 0x03
        uint8_t var_04;
        uint8_t trackType;                                    // 0x05
        uint8_t numTrackExtras;                               // 0x06
        uint8_t costIndex;                                    // 0x07
        int16_t costFactor;                                   // 0x08
        uint8_t reliability;                                  // 0x0A
        uint8_t runCostIndex;                                 // 0x0B
        int16_t runCostFactor;                                // 0x0C
        uint8_t colourType;                                   // 0x0E
        uint8_t numCompatibleVehicles;                        // 0x0F
        uint16_t compatibleVehicles[8];                       // 0x10 array of compatible vehicle_types
        uint8_t requiredTrackExtras[4];                       // 0x20
        VehicleObjectCar carComponents[kMaxCarComponents];    // 0x24
        VehicleObjectBodySprite bodySprites[kMaxBodySprites]; // 0x3C
        VehicleObjectBogieSprite bogieSprites[2];             // 0xB4
        uint16_t power;                                       // 0xD8
        Speed16 speed;                                        // 0xDA
        Speed16 rackSpeed;                                    // 0xDC
        uint16_t weight;                                      // 0xDE
        VehicleObjectFlags flags;                             // 0xE0
        uint8_t maxCargo[2];                                  // 0xE2 size is relative to the first compatibleCargoCategories
        uint32_t compatibleCargoCategories[2];                // 0xE4
        uint8_t cargoTypeSpriteOffsets[32];                   // 0xEC
        uint8_t numSimultaneousCargoTypes;                    // 0x10C
        VehicleObjectSimpleAnimation animation[2];            // 0x10D
        uint8_t var_113;
        uint16_t designed;                 // 0x114
        uint16_t obsolete;                 // 0x116
        uint8_t rackRailType;              // 0x118
        DrivingSoundType drivingSoundType; // 0x119
        union
        {
            VehicleObjectFrictionSound friction;
            VehicleSimpleMotorSound simpleMotor;
            VehicleGearboxMotorSound gearboxMotor;
        } sound;
        uint8_t pad_135[0x15A - 0x135];
        uint8_t numStartSounds;                       // 0x15A use mask when accessing kHasCrossingWhistle stuffed in (1 << 7)
        SoundObjectId_t startSounds[kMaxStartSounds]; // 0x15B sound array length numStartSounds highest sound is the crossing whistle

        void drawPreviewImage(Gfx::DrawingContext& drawingCtx, const int16_t x, const int16_t y) const;
        void drawDescription(Gfx::DrawingContext& drawingCtx, const int16_t x, const int16_t y, const int16_t width) const;
        void getCargoString(char* buffer) const;
        bool validate() const;
        void load(const LoadedObjectHandle& handle, std::span<const std::byte> data, ObjectManager::DependentObjects* dependencies);
        void unload();
        uint32_t getLength() const;
        constexpr bool hasFlags(VehicleObjectFlags flagsToTest) const
        {
            return (flags & flagsToTest) != VehicleObjectFlags::none;
        }
    };
#pragma pack(pop)
    static_assert(sizeof(VehicleObject) == 0x15E);

    namespace StringIds
    {
        constexpr StringId getVehicleType(const VehicleType type)
        {
            switch (type)
            {
                case VehicleType::train:
                    return StringIds::train;
                case VehicleType::bus:
                    return StringIds::bus;
                case VehicleType::truck:
                    return StringIds::truck;
                case VehicleType::tram:
                    return StringIds::tram;
                case VehicleType::aircraft:
                    return StringIds::aircraft;
                case VehicleType::ship:
                    return StringIds::ship;
            }
            return StringIds::empty;
        }
    }
}
