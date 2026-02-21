// AndroidAssets.cpp - Extract APK assets to internal storage on Android
// (C) 2025

#ifdef __ANDROID__

#include <SDL3/SDL.h>
#include <SDL3/SDL_system.h>
#include <string>
#include <filesystem>
#include <fstream>
#include <android/log.h>
#include <stdlib.h>

namespace fs = std::filesystem;

#define TAG "Nanosaur2"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,  TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, TAG, __VA_ARGS__)

// Increment this to force re-extraction after data updates
#define ASSET_VERSION "2"

// Complete hardcoded list of all game data files
static const char* kAllDataFiles[] = {
    "Audio/Main/BadSelect.aiff",
    "Audio/Main/BodyHit.aiff",
    "Audio/Main/BombDrop.aiff",
    "Audio/Main/BrachDeath.aiff",
    "Audio/Main/BrachHurt.aiff",
    "Audio/Main/ChangeSelect.aiff",
    "Audio/Main/ChangeWeapon.aiff",
    "Audio/Main/CrystalShatter.aiff",
    "Audio/Main/Dirt.aiff",
    "Audio/Main/DustDevil.aiff",
    "Audio/Main/EggIntoWormhole.aiff",
    "Audio/Main/ElectrodeHum.aiff",
    "Audio/Main/FlareShoot.aiff",
    "Audio/Main/GetPOW.aiff",
    "Audio/Main/GrabEgg.aiff",
    "Audio/Main/ImpactSizzle.aiff",
    "Audio/Main/JetpackHum.aiff",
    "Audio/Main/JetpackIgnite.aiff",
    "Audio/Main/LaserBeam.aiff",
    "Audio/Main/LaunchMissile.aiff",
    "Audio/Main/MenuSelect.aiff",
    "Audio/Main/MineExplode.aiff",
    "Audio/Main/MissileEngine.aiff",
    "Audio/Main/PlaneCrash.aiff",
    "Audio/Main/RaptorAttack.aiff",
    "Audio/Main/RaptorDeath.aiff",
    "Audio/Main/RocketLaunch.aiff",
    "Audio/Main/Shield.aiff",
    "Audio/Main/SonicScream.aiff",
    "Audio/Main/Splash.aiff",
    "Audio/Main/StunGun.aiff",
    "Audio/Main/TurretExplosion.aiff",
    "Audio/Main/TurretFire.aiff",
    "Audio/Main/WeaponCharge.aiff",
    "Audio/Main/Wormhole.aiff",
    "Audio/Main/WormholeAppear.aiff",
    "Audio/Main/WormholeVanish.aiff",
    "Audio/Narration/story1.mp3",
    "Audio/Narration/story2.mp3",
    "Audio/Narration/story3.mp3",
    "Audio/Narration/story4.mp3",
    "Audio/Narration/story5.mp3",
    "Audio/Narration/story6.mp3",
    "Audio/Narration/story7.mp3",
    "Audio/introsong.mp3",
    "Audio/level1song.mp3",
    "Audio/level2song.mp3",
    "Audio/level3song.mp3",
    "Audio/theme.mp3",
    "Audio/winsong.mp3",
    "Models/desert.bg3d",
    "Models/forest.bg3d",
    "Models/global.bg3d",
    "Models/levelintro.bg3d",
    "Models/playerparts.bg3d",
    "Models/swamp.bg3d",
    "Models/weapons.bg3d",
    "Skeletons/bonusworm.bg3d",
    "Skeletons/bonusworm.skeleton.rsrc",
    "Skeletons/brach.bg3d",
    "Skeletons/brach.skeleton.rsrc",
    "Skeletons/nano.bg3d",
    "Skeletons/nano.skeleton.rsrc",
    "Skeletons/ramphor.bg3d",
    "Skeletons/ramphor.skeleton.rsrc",
    "Skeletons/raptor.bg3d",
    "Skeletons/raptor.skeleton.rsrc",
    "Skeletons/worm.bg3d",
    "Skeletons/worm.skeleton.rsrc",
    "Skeletons/wormhole.bg3d",
    "Skeletons/wormhole.skeleton.rsrc",
    "Sprites/calibration/calibration000.png",
    "Sprites/calibration/calibration001.jpg",
    "Sprites/calibration/calibration002.jpg",
    "Sprites/calibration/calibration003.jpg",
    "Sprites/calibration/glasses.jpg",
    "Sprites/calibration/glasses.png",
    "Sprites/fonts/font.alt1.png",
    "Sprites/fonts/font.kerning.txt",
    "Sprites/fonts/font.png",
    "Sprites/fonts/font.txt",
    "Sprites/fonts/swiss.png",
    "Sprites/fonts/swiss.txt",
    "Sprites/global/global000.png",
    "Sprites/global/global001.png",
    "Sprites/global/global002.png",
    "Sprites/global/global003.png",
    "Sprites/global/global004.jpg",
    "Sprites/global/global005.jpg",
    "Sprites/global/global005.png",
    "Sprites/global/global006.jpg",
    "Sprites/global/global006.png",
    "Sprites/global/global007.jpg",
    "Sprites/global/global008.jpg",
    "Sprites/infobar/infobar000.png",
    "Sprites/infobar/infobar001.png",
    "Sprites/infobar/infobar002.png",
    "Sprites/infobar/infobar003.png",
    "Sprites/infobar/infobar004.png",
    "Sprites/infobar/infobar005.png",
    "Sprites/infobar/infobar006.png",
    "Sprites/infobar/infobar007.png",
    "Sprites/infobar/infobar008.png",
    "Sprites/infobar/infobar009.png",
    "Sprites/infobar/infobar010.png",
    "Sprites/infobar/infobar011.jpg",
    "Sprites/infobar/infobar011.png",
    "Sprites/infobar/infobar012.jpg",
    "Sprites/infobar/infobar012.png",
    "Sprites/infobar/infobar013.jpg",
    "Sprites/infobar/infobar013.png",
    "Sprites/infobar/infobar014.jpg",
    "Sprites/infobar/infobar014.png",
    "Sprites/infobar/infobar015.jpg",
    "Sprites/infobar/infobar015.png",
    "Sprites/infobar/infobar016.jpg",
    "Sprites/infobar/infobar016.png",
    "Sprites/infobar/infobar017.jpg",
    "Sprites/infobar/infobar017.png",
    "Sprites/infobar/infobar018.jpg",
    "Sprites/infobar/infobar018.png",
    "Sprites/infobar/infobar019.jpg",
    "Sprites/infobar/infobar019.png",
    "Sprites/infobar/infobar020.png",
    "Sprites/infobar/infobar021.jpg",
    "Sprites/infobar/infobar021.png",
    "Sprites/infobar/infobar022.jpg",
    "Sprites/infobar/infobar022.png",
    "Sprites/infobar/infobar023.jpg",
    "Sprites/infobar/infobar023.png",
    "Sprites/infobar/infobar024.jpg",
    "Sprites/infobar/infobar024.png",
    "Sprites/infobar/infobar025.jpg",
    "Sprites/infobar/infobar025.png",
    "Sprites/infobar/infobar026.jpg",
    "Sprites/infobar/infobar026.png",
    "Sprites/infobar/infobar027.jpg",
    "Sprites/infobar/infobar027.png",
    "Sprites/infobar/infobar028.jpg",
    "Sprites/infobar/infobar028.png",
    "Sprites/infobar/infobar029.jpg",
    "Sprites/infobar/infobar029.png",
    "Sprites/infobar/infobar030.jpg",
    "Sprites/infobar/infobar030.png",
    "Sprites/infobar/infobar031.jpg",
    "Sprites/infobar/infobar031.png",
    "Sprites/infobar/infobar032.jpg",
    "Sprites/infobar/infobar032.png",
    "Sprites/infobar/infobar033.jpg",
    "Sprites/infobar/infobar033.png",
    "Sprites/infobar/infobar034.jpg",
    "Sprites/infobar/infobar034.png",
    "Sprites/infobar/infobar035.jpg",
    "Sprites/infobar/infobar035.png",
    "Sprites/infobar/infobar036.jpg",
    "Sprites/infobar/infobar036.png",
    "Sprites/infobar/infobar037.jpg",
    "Sprites/infobar/infobar037.png",
    "Sprites/infobar/infobar038.jpg",
    "Sprites/infobar/infobar038.png",
    "Sprites/infobar/infobar039.jpg",
    "Sprites/infobar/infobar039.png",
    "Sprites/infobar/infobar040.jpg",
    "Sprites/infobar/infobar040.png",
    "Sprites/infobar/infobar041.jpg",
    "Sprites/infobar/infobar041.png",
    "Sprites/infobar/infobar042.jpg",
    "Sprites/infobar/infobar042.png",
    "Sprites/infobar/infobar043.jpg",
    "Sprites/infobar/infobar043.png",
    "Sprites/infobar/infobar044.jpg",
    "Sprites/infobar/infobar044.png",
    "Sprites/infobar/infobar045.jpg",
    "Sprites/infobar/infobar045.png",
    "Sprites/infobar/infobar046.jpg",
    "Sprites/infobar/infobar046.png",
    "Sprites/infobar/infobar047.jpg",
    "Sprites/infobar/infobar048.png",
    "Sprites/infobar/infobar049.jpg",
    "Sprites/infobar/infobar049.png",
    "Sprites/infobar/infobar050.png",
    "Sprites/infobar/infobar051.png",
    "Sprites/infobar/infobar052.png",
    "Sprites/infobar/infobar053.jpg",
    "Sprites/infobar/infobar053.png",
    "Sprites/infobar/infobar054.jpg",
    "Sprites/infobar/infobar055.png",
    "Sprites/infobar/infobar056.jpg",
    "Sprites/infobar/infobar056.png",
    "Sprites/infobar/infobar057.jpg",
    "Sprites/infobar/infobar057.png",
    "Sprites/infobar/infobar058.jpg",
    "Sprites/infobar/infobar059.png",
    "Sprites/infobar/infobar060.jpg",
    "Sprites/infobar/infobar060.png",
    "Sprites/infobar/infobar061.jpg",
    "Sprites/infobar/infobar061.png",
    "Sprites/maps/battle1.jpg",
    "Sprites/maps/battle1.png",
    "Sprites/maps/battle2.jpg",
    "Sprites/maps/battle2.png",
    "Sprites/maps/flag1.jpg",
    "Sprites/maps/flag1.png",
    "Sprites/maps/flag2.jpg",
    "Sprites/maps/flag2.png",
    "Sprites/maps/level1.jpg",
    "Sprites/maps/level1.png",
    "Sprites/maps/level2.jpg",
    "Sprites/maps/level2.png",
    "Sprites/maps/level3.jpg",
    "Sprites/maps/level3.png",
    "Sprites/maps/race1.jpg",
    "Sprites/maps/race1.png",
    "Sprites/maps/race2.jpg",
    "Sprites/maps/race2.png",
    "Sprites/menu/cursor.png",
    "Sprites/menu/menuback.jpg",
    "Sprites/menu/nanologo.jpg",
    "Sprites/menu/nanologo.png",
    "Sprites/particle/particle000.jpg",
    "Sprites/particle/particle001.jpg",
    "Sprites/particle/particle002.jpg",
    "Sprites/particle/particle003.jpg",
    "Sprites/particle/particle004.jpg",
    "Sprites/particle/particle005.jpg",
    "Sprites/particle/particle006.jpg",
    "Sprites/particle/particle007.jpg",
    "Sprites/particle/particle008.jpg",
    "Sprites/particle/particle009.jpg",
    "Sprites/particle/particle010.jpg",
    "Sprites/particle/particle011.jpg",
    "Sprites/particle/particle012.jpg",
    "Sprites/particle/particle012.png",
    "Sprites/particle/particle013.jpg",
    "Sprites/particle/particle013.png",
    "Sprites/particle/particle014.jpg",
    "Sprites/particle/particle014.png",
    "Sprites/particle/particle015.jpg",
    "Sprites/particle/particle015.png",
    "Sprites/particle/particle016.jpg",
    "Sprites/particle/particle016.png",
    "Sprites/particle/particle017.jpg",
    "Sprites/particle/particle018.jpg",
    "Sprites/particle/particle019.jpg",
    "Sprites/particle/particle020.jpg",
    "Sprites/particle/particle020.png",
    "Sprites/particle/particle021.jpg",
    "Sprites/particle/particle021.png",
    "Sprites/particle/particle022.jpg",
    "Sprites/particle/particle022.png",
    "Sprites/particle/particle023.jpg",
    "Sprites/particle/particle023.png",
    "Sprites/particle/particle024.jpg",
    "Sprites/particle/particle024.png",
    "Sprites/particle/particle025.jpg",
    "Sprites/particle/particle026.jpg",
    "Sprites/particle/particle027.jpg",
    "Sprites/particle/particle028.jpg",
    "Sprites/particle/particle029.jpg",
    "Sprites/particle/particle030.jpg",
    "Sprites/particle/particle030.png",
    "Sprites/particle/particle031.jpg",
    "Sprites/particle/particle031.png",
    "Sprites/particle/particle032.jpg",
    "Sprites/particle/particle032.png",
    "Sprites/particle/particle033.jpg",
    "Sprites/particle/particle033.png",
    "Sprites/particle/particle034.jpg",
    "Sprites/particle/particle034.png",
    "Sprites/particle/particle035.jpg",
    "Sprites/particle/particle035.png",
    "Sprites/particle/particle036.jpg",
    "Sprites/particle/particle036.png",
    "Sprites/particle/particle037.jpg",
    "Sprites/particle/particle037.png",
    "Sprites/particle/particle038.jpg",
    "Sprites/particle/particle038.png",
    "Sprites/particle/particle039.jpg",
    "Sprites/particle/particle039.png",
    "Sprites/particle/particle040.jpg",
    "Sprites/particle/particle040.png",
    "Sprites/particle/particle041.jpg",
    "Sprites/particle/particle041.png",
    "Sprites/particle/particle042.jpg",
    "Sprites/spheremap/spheremap000.jpg",
    "Sprites/spheremap/spheremap001.jpg",
    "Sprites/spheremap/spheremap002.jpg",
    "Sprites/spheremap/spheremap003.jpg",
    "Sprites/spheremap/spheremap004.jpg",
    "Sprites/spheremap/spheremap005.jpg",
    "Sprites/spheremap/spheremap006.jpg",
    "Sprites/spheremap/spheremap007.jpg",
    "Sprites/spheremap/spheremap008.jpg",
    "Sprites/story/story000.jpg",
    "Sprites/story/story000.png",
    "Sprites/story/story001.jpg",
    "Sprites/story/story002.jpg",
    "Sprites/story/story003.jpg",
    "Sprites/story/story004.jpg",
    "Sprites/story/story005.jpg",
    "Sprites/story/story006.jpg",
    "Sprites/story/story007.jpg",
    "Sprites/story/story008.jpg",
    "Sprites/story/win.jpg",
    "Sprites/textures/blockenemy.png",
    "Sprites/textures/dustdevil.jpg",
    "Sprites/textures/dustdevil.png",
    "Sprites/textures/pinefence.jpg",
    "Sprites/textures/pinefence.png",
    "Sprites/textures/player2.jpg",
    "Sprites/textures/player2.png",
    "Sprites/textures/stardome.jpg",
    "System/gamecontrollerdb.txt",
    "System/strings.csv",
    "System/twitch.csv",
    "Terrain/battle1.ter",
    "Terrain/battle1.ter.rsrc",
    "Terrain/battle2.ter",
    "Terrain/battle2.ter.rsrc",
    "Terrain/flag1.ter",
    "Terrain/flag1.ter.rsrc",
    "Terrain/flag2.ter",
    "Terrain/flag2.ter.rsrc",
    "Terrain/level1.ter",
    "Terrain/level1.ter.rsrc",
    "Terrain/level2.ter",
    "Terrain/level2.ter.rsrc",
    "Terrain/level3.ter",
    "Terrain/level3.ter.rsrc",
    "Terrain/race1.ter",
    "Terrain/race1.ter.rsrc",
    "Terrain/race2.ter",
    "Terrain/race2.ter.rsrc",
    NULL  // sentinel
};

static bool EnsureDir(const char* path) {
    std::error_code ec;
    fs::create_directories(path, ec);
    return !ec;
}

static bool CopyAssetFile(const char* relPath, const char* destBase) {
    // APK asset path: assets.srcDirs("../../Data") places Data/ contents at APK root,
    // so the asset is accessible as just relPath (e.g. "System/gamecontrollerdb.txt"),
    // NOT as "Data/System/gamecontrollerdb.txt".
    const char* srcPath = relPath;
    char destPath[1024];
    snprintf(destPath, sizeof(destPath), "%s/Data/%s", destBase, relPath);

    // Create parent directories
    {
        char dirBuf[1024];
        snprintf(dirBuf, sizeof(dirBuf), "%s", destPath);
        char *lastSlash = strrchr(dirBuf, '/');
        if (lastSlash) {
            *lastSlash = '\0';
            EnsureDir(dirBuf);
        }
    }

    // Open source from APK assets
    SDL_IOStream* src = SDL_IOFromFile(srcPath, "rb");
    if (!src) {
        LOGE("Asset not found: %s", srcPath);
        return false;
    }

    // Open destination
    FILE* dst = fopen(destPath, "wb");
    if (!dst) {
        SDL_CloseIO(src);
        LOGE("Cannot open dest: %s", destPath);
        return false;
    }

    // Copy in chunks
    char buf[65536];
    bool ok = true;
    while (true) {
        size_t n = SDL_ReadIO(src, buf, sizeof(buf));
        if (n == 0) break;
        if (fwrite(buf, 1, n, dst) != n) {
            LOGE("Write error: %s", destPath);
            ok = false;
            break;
        }
    }

    fclose(dst);
    SDL_CloseIO(src);
    return ok;
}

extern "C" bool AndroidExtractAssets(void) {
    const char* internalPath = SDL_GetAndroidInternalStoragePath();
    if (!internalPath) {
        LOGE("Cannot get internal storage path");
        return false;
    }

    LOGI("Internal storage: %s", internalPath);

    // Check version stamp
    std::string stampPath = std::string(internalPath) + "/.asset_version";
    {
        std::ifstream stamp(stampPath);
        if (stamp.is_open()) {
            std::string ver;
            std::getline(stamp, ver);
            if (ver == ASSET_VERSION) {
                LOGI("Assets already extracted (v%s)", ASSET_VERSION);
                return true;
            }
        }
    }

    LOGI("Extracting %d game assets...", (int)(sizeof(kAllDataFiles)/sizeof(kAllDataFiles[0]) - 1));

    // Create Data directory
    std::string dataDir = std::string(internalPath) + "/Data";
    EnsureDir(dataDir.c_str());

    int failures = 0;
    for (int i = 0; kAllDataFiles[i]; i++) {
        if (!CopyAssetFile(kAllDataFiles[i], internalPath)) {
            failures++;
        }
    }

    if (failures > 0) {
        LOGE("%d files failed to extract", failures);
        // Don't write stamp - retry next time
        return false;
    }

    // Write version stamp
    {
        std::ofstream stamp(stampPath);
        stamp << ASSET_VERSION;
    }

    LOGI("Assets extracted successfully");
    return true;
}

#endif // __ANDROID__
