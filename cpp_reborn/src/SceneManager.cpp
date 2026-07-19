#include "SceneManager.h"
#include "FileLoader.h"
#include "GraphicsUtils.h"
#include "GameManager.h"
#include "TextManager.h"
#include <SDL3_image/SDL_image.h>
#include <iostream>
#include <cstring>
#include <algorithm>

SceneManager& SceneManager::getInstance() {
    static SceneManager instance;
    return instance;
}

SceneManager::SceneManager() : m_currentSceneId(0) {}

bool SceneManager::Init() {
    // Load resources
    if (!LoadResources()) {
        std::cerr << "Warning: Failed to load tile resources (smp/sdx)" << std::endl;
        // Don't fail completely, maybe resources are missing but we can still load map logic
    }
    // 5. 加载战斗地图资源 (WarMap) - wmp/wdx
    m_wmpPicData = FileLoader::loadFile("resource/wmp");
    auto wIdxBytes = FileLoader::loadFile("resource/wdx");
    
    if (!m_wmpPicData.empty() && !wIdxBytes.empty()) {
        size_t count = wIdxBytes.size() / 4;
        m_wmpIdxData.resize(count);
        memcpy(m_wmpIdxData.data(), wIdxBytes.data(), wIdxBytes.size());
        std::cout << "[SceneManager] Loaded WarMap (wmp/wdx). Pic Data: " << m_wmpPicData.size() << " bytes, Idx Count: " << count << std::endl;
    } else {
        std::cerr << "[SceneManager] Failed to load WarMap (wmp/wdx)" << std::endl;
    }
    
    // 6. Load World Map Layout (EARTH.002)
    LoadWorldMap();

    return true;
}

bool SceneManager::LoadWorldMap() {
    auto loadLayer = [](const std::string& filename) -> std::vector<int16_t> {
        auto data = FileLoader::loadFile(filename);
        if (data.empty()) {
            auto alt = FileLoader::loadFile("resource/" + filename);
            if (alt.empty()) {
                return {};
            }
            data.swap(alt);
        }
        if (data.size() != 480 * 480 * 2) {
            std::cerr << "[LoadWorldMap] Warning: " << filename
                      << " size mismatch, expect " << (480 * 480 * 2)
                      << " bytes, got " << data.size() << std::endl;
        }
        std::vector<int16_t> layer(data.size() / 2);
        memcpy(layer.data(), data.data(), data.size());
        return layer;
    };

    m_worldEarth    = loadLayer("EARTH.002");
    m_worldSurface  = loadLayer("surface.002");
    m_worldBuilding = loadLayer("building.002");
    m_worldBuildX   = loadLayer("buildx.002");
    m_worldBuildY   = loadLayer("buildy.002");

    if (m_worldEarth.empty()) {
        std::cerr << "[LoadWorldMap] Failed to load EARTH.002" << std::endl;
        return false;
    }

    if (m_worldSurface.empty() || m_worldBuilding.empty() ||
        m_worldBuildX.empty() || m_worldBuildY.empty()) {
        std::cerr << "[LoadWorldMap] Warning: some world layers missing. "
                  << "Surface=" << m_worldSurface.size()
                  << " Building=" << m_worldBuilding.size()
                  << " BuildX=" << m_worldBuildX.size()
                  << " BuildY=" << m_worldBuildY.size() << std::endl;
    } else {
        std::cout << "[LoadWorldMap] World layers loaded. "
                  << "Earth=" << m_worldEarth.size()
                  << " Surface=" << m_worldSurface.size()
                  << " Building=" << m_worldBuilding.size()
                  << " BuildX=" << m_worldBuildX.size()
                  << " BuildY=" << m_worldBuildY.size() << std::endl;
    }

    return true;
}

bool SceneManager::LoadResources() {


    // 1. 加载场景图块资源 (SceneMap) - smp/sdx
    m_smpPicData = FileLoader::loadFile("resource/smp");
    auto idxBytes = FileLoader::loadFile("resource/sdx");
    
    if (!m_smpPicData.empty() && !idxBytes.empty()) {
        size_t count = idxBytes.size() / 4;
        m_smpIdxData.resize(count);
        memcpy(m_smpIdxData.data(), idxBytes.data(), idxBytes.size());
        std::cout << "[SceneManager] Loaded SceneMap (smp/sdx) with " << count << " tiles." << std::endl;
        
        // Debug: Print first few offsets to verify loading
        std::cout << "  First 5 Offsets: ";
        for(int i=0; i<5 && i<count; ++i) std::cout << m_smpIdxData[i] << " ";
        std::cout << std::endl;
        
        // Check offset for Player Sprite (approx 2501)
        if (count > 2501) {
             std::cout << "  Player Offset (2501): " << m_smpIdxData[2501] << std::endl;
        }
        
    } else {
        std::cerr << "Failed to load SceneMap (smp/sdx)" << std::endl;
        return false; 
    }
    
    // 2. 加载大地图贴图资源 (MaxMap) - mmap.grp/mmap.idx
    m_mmpPicData = FileLoader::loadFile("resource/mmap.grp");
    auto mmapIdxBytes = FileLoader::loadFile("resource/mmap.idx");
    
    if (!m_mmpPicData.empty() && !mmapIdxBytes.empty()) {
        size_t count = mmapIdxBytes.size() / 4;
        m_mmpIdxData.resize(count);
        memcpy(m_mmpIdxData.data(), mmapIdxBytes.data(), mmapIdxBytes.size());
        std::cout << "[SceneManager] Loaded MaxMap (mmp/midx) with " << count << " sprites." << std::endl;
    } else {
        std::cerr << "Failed to load MaxMap (mmap.grp/mmap.idx)" << std::endl;
    }

    // 2.5 加载动态场景贴图资源 (Scene.Pic)
    auto scenePicData = FileLoader::loadFile("resource/Scene.Pic");
    if (!scenePicData.empty()) {
        const uint8_t* ptr = scenePicData.data();
        int32_t count;
        memcpy(&count, ptr, 4);
        ptr += 4;
        
        m_scenePics.resize(count);
        for (int i = 0; i < count; ++i) {
            int32_t offset;
            memcpy(&offset, scenePicData.data() + (i + 1) * 4, 4);
            
            const uint8_t* picPtr = scenePicData.data() + offset;
            memcpy(&m_scenePics[i].x, picPtr, 4);
            memcpy(&m_scenePics[i].y, picPtr + 4, 4);
            memcpy(&m_scenePics[i].black, picPtr + 8, 4);
            
            // The rest is PNG data
            // Length calculation: next_offset - current_offset - 12
            int32_t nextOffset;
            if (i < count - 1) {
                memcpy(&nextOffset, scenePicData.data() + (i + 2) * 4, 4);
            } else {
                nextOffset = scenePicData.size();
            }
            int32_t pngLen = nextOffset - offset - 12;
            
            if (pngLen > 0) {
                SDL_IOStream* stream = SDL_IOFromConstMem(picPtr + 12, pngLen);
                if (stream) {
                    m_scenePics[i].surface = IMG_LoadTyped_IO(stream, true, "PNG");
                }
            }
        }
        std::cout << "[SceneManager] Loaded Scene.Pic with " << count << " sprites." << std::endl;
    } else {
        std::cerr << "Failed to load Scene.Pic" << std::endl;
    }

    // 3. Load Cloud Graphics (cloud.grp/cloud.idx)
    m_cloudPicData = FileLoader::loadFile("resource/cloud.grp");
    auto cloudIdxBytes = FileLoader::loadFile("resource/cloud.idx");
    
    if (!m_cloudPicData.empty() && !cloudIdxBytes.empty()) {
        size_t count = cloudIdxBytes.size() / 4;
        m_cloudIdxData.resize(count);
        memcpy(m_cloudIdxData.data(), cloudIdxBytes.data(), cloudIdxBytes.size());
    } else {
        std::cerr << "Failed to load cloud.grp/cloud.idx" << std::endl;
    }

    // 4. Load Palette (MMAP.COL)
    std::string palPath = FileLoader::getResourcePath("resource/MMAP.COL");
    GraphicsUtils::loadPalette(palPath);
    
    return true;
}

bool SceneManager::LoadEventData(const std::string& path) {
    auto data = FileLoader::loadFile(path);
    if (data.empty()) return false;

    // DData size per scene: 200 events * 11 ints * 2 bytes = 4400 bytes
    size_t sceneSize = 200 * 11 * 2;
    if (data.size() % sceneSize != 0) {
        std::cerr << "Warning: Event data size mismatch. Expected multiple of " << sceneSize << ", got " << data.size() << std::endl;
    }

    size_t count = data.size() / sceneSize;
    m_eventData.resize(count);
    
    for (size_t i = 0; i < count; ++i) {
        memcpy(m_eventData[i].data, data.data() + i * sceneSize, sceneSize);
    }

    // Debug: Check Scene 0 Data immediately after load
    if (count > 0) {
        std::cout << "[LoadEventData] Loaded " << count << " scenes. Checking Scene 0..." << std::endl;
        bool hasActive = false;
        for (int e = 0; e < 200; ++e) {
             if (m_eventData[0].data[e][0] != 0) {
                 hasActive = true;
                 if (e < 5) { // Print first few active events
                     std::cout << "  Event " << e << " Active=" << m_eventData[0].data[e][0] 
                               << " X=" << m_eventData[0].data[e][1] 
                               << " Y=" << m_eventData[0].data[e][2] 
                               << " Pic=" << m_eventData[0].data[e][5] << std::endl;
                 }
             }
        }
        if (!hasActive) {
            std::cerr << "[LoadEventData] WARNING: Scene 0 has NO active events! The file might be empty or zeroed." << std::endl;
            // Hex dump first 32 bytes
            std::cout << "  Raw Hex Dump (First 32 bytes): ";
            for(int k=0; k<32 && k<data.size(); ++k) {
                printf("%02X ", data[k]);
            }
            std::cout << std::endl;
        }
    }
    
    // Debug: Check Scene 0, Event 3 (Kong Pili?)
    if (count > 0) {
        std::cout << "[LoadEventData] Checking Scene 0, Event 3:" << std::endl;
        for(int k=0; k<11; ++k) {
            std::cout << "  Index " << k << ": " << m_eventData[0].data[3][k] << std::endl;
        }
        // Index 5 is supposed to be Pic. If it's 0, that's why it's invisible.
    }
    
    std::cout << "Loaded " << count << " scenes event data from " << path << std::endl;
    return true;
}

bool SceneManager::SaveEventData(const std::string& path) {
    if (m_eventData.empty()) return false;
    
    // DData size per scene: 200 events * 11 ints * 2 bytes = 4400 bytes
    size_t sceneSize = 200 * 11 * 2;
    size_t totalSize = m_eventData.size() * sceneSize;
    
    std::vector<uint8_t> buffer(totalSize);
    for (size_t i = 0; i < m_eventData.size(); ++i) {
        memcpy(buffer.data() + i * sceneSize, m_eventData[i].data, sceneSize);
    }
    
    return FileLoader::saveFile(path, buffer.data(), buffer.size());
}

bool SceneManager::LoadMapData(const std::string& path) {
    auto data = FileLoader::loadFile(path);
    if (data.empty()) return false;
    
    // SData size per scene: 6 layers * 64 * 64 tiles * 2 bytes/tile
    size_t sceneSize = SCENE_LAYERS * SCENE_MAP_SIZE * SCENE_MAP_SIZE * 2;
    if (data.size() % sceneSize != 0) {
        std::cerr << "Warning: Map data size mismatch. Expected multiple of " << sceneSize << ", got " << data.size() << std::endl;
    }
    
    m_mapData.resize(data.size() / 2);
    memcpy(m_mapData.data(), data.data(), data.size());
    return true;
}

bool SceneManager::SaveMapData(const std::string& path) {
    if (m_mapData.empty()) return false;
    
    // Convert int16 vector to bytes
    size_t byteSize = m_mapData.size() * 2;
    return FileLoader::saveFile(path, m_mapData.data(), byteSize);
}

void SceneManager::SetScenes(const std::vector<Scene>& scenes) {
    m_scenes = scenes;
    std::cout << "SceneManager: Set " << m_scenes.size() << " scenes." << std::endl;
    ResetEntrance();
}

void SceneManager::SetCurrentScene(int sceneId) {
    m_currentSceneId = sceneId;
}

Scene* SceneManager::GetScene(int sceneId) {
    if (sceneId < 0 || sceneId >= m_scenes.size()) return nullptr;
    return &m_scenes[sceneId];
}

size_t SceneManager::GetMapSceneCount() const {
    if (m_mapData.empty()) return 0;
    size_t sceneSize = SCENE_LAYERS * SCENE_MAP_SIZE * SCENE_MAP_SIZE;
    return m_mapData.size() / sceneSize;
}

int16_t SceneManager::GetSceneTile(int sceneId, int layer, int x, int y) const {
    if (sceneId < 0 || layer < 0 || layer >= SCENE_LAYERS || x < 0 || x >= SCENE_MAP_SIZE || y < 0 || y >= SCENE_MAP_SIZE) {
        return -1;
    }
    
    size_t sceneOffset = sceneId * (SCENE_LAYERS * SCENE_MAP_SIZE * SCENE_MAP_SIZE);
    size_t layerOffset = layer * (SCENE_MAP_SIZE * SCENE_MAP_SIZE);
    // Transposed access: x * 64 + y
    size_t tileOffset = x * SCENE_MAP_SIZE + y;
    
    size_t index = sceneOffset + layerOffset + tileOffset;
    if (index >= m_mapData.size()) return -1;
    
    return m_mapData[index];
}

void SceneManager::SetSceneTile(int sceneId, int layer, int x, int y, int16_t value) {
    if (sceneId < 0 || layer < 0 || layer >= SCENE_LAYERS || x < 0 || x >= SCENE_MAP_SIZE || y < 0 || y >= SCENE_MAP_SIZE) {
        return;
    }
    
    size_t sceneOffset = sceneId * (SCENE_LAYERS * SCENE_MAP_SIZE * SCENE_MAP_SIZE);
    size_t layerOffset = layer * (SCENE_MAP_SIZE * SCENE_MAP_SIZE);
    // Transposed access: x * 64 + y
    size_t tileOffset = x * SCENE_MAP_SIZE + y;
    
    size_t index = sceneOffset + layerOffset + tileOffset;
    if (index >= m_mapData.size()) return;
    
    m_mapData[index] = value;
}

int16_t SceneManager::GetWorldEarth(int x, int y) const {
    if (x < 0 || x >= m_worldMapWidth || y < 0 || y >= m_worldMapHeight) return 0;
    size_t idx = static_cast<size_t>(x) * m_worldMapWidth + y;
    if (idx >= m_worldEarth.size()) return 0;
    return m_worldEarth[idx];
}

int16_t SceneManager::GetWorldSurface(int x, int y) const {
    if (x < 0 || x >= m_worldMapWidth || y < 0 || y >= m_worldMapHeight) return 0;
    size_t idx = static_cast<size_t>(x) * m_worldMapWidth + y;
    if (idx >= m_worldSurface.size()) return 0;
    return m_worldSurface[idx];
}

int16_t SceneManager::GetWorldBuildX(int x, int y) const {
    if (x < 0 || x >= m_worldMapWidth || y < 0 || y >= m_worldMapHeight) return 0;
    size_t idx = static_cast<size_t>(x) * m_worldMapWidth + y;
    if (idx >= m_worldBuildX.size()) return 0;
    return m_worldBuildX[idx];
}

int16_t SceneManager::GetWorldBuildY(int x, int y) const {
    if (x < 0 || x >= m_worldMapWidth || y < 0 || y >= m_worldMapHeight) return 0;
    size_t idx = static_cast<size_t>(x) * m_worldMapWidth + y;
    if (idx >= m_worldBuildY.size()) return 0;
    return m_worldBuildY[idx];
}

void SceneManager::ResetEntrance() {
    m_worldEntrance.assign(m_worldMapWidth * m_worldMapHeight, -1);
    std::cout << "[ResetEntrance] Resetting " << m_scenes.size() << " scenes entrances..." << std::endl;
    for (int i = 0; i < static_cast<int>(m_scenes.size()); ++i) {
        const Scene& scene = m_scenes[i];
        int x1 = scene.getMainEntranceX1();
        int y1 = scene.getMainEntranceY1();
        std::cout << "[ResetEntrance] Scene " << i << " (" << scene.getName() << ") Entrance1: (" << x1 << "," << y1 << ")" << std::endl;
        if (x1 >= 0 && x1 < m_worldMapWidth && y1 >= 0 && y1 < m_worldMapHeight) {
            size_t idx = static_cast<size_t>(x1) * m_worldMapWidth + y1;
            if (idx < m_worldEntrance.size()) {
                m_worldEntrance[idx] = static_cast<int16_t>(i);
                std::cout << "[ResetEntrance]   -> Set entrance at (" << x1 << "," << y1 << ") idx=" << idx << std::endl;
            }
        }
        int x2 = scene.getMainEntranceX2();
        int y2 = scene.getMainEntranceY2();
        if (x2 >= 0 && x2 < m_worldMapWidth && y2 >= 0 && y2 < m_worldMapHeight) {
            size_t idx = static_cast<size_t>(x2) * m_worldMapWidth + y2;
            if (idx < m_worldEntrance.size()) {
                m_worldEntrance[idx] = static_cast<int16_t>(i);
                std::cout << "[ResetEntrance]   -> Set entrance2 at (" << x2 << "," << y2 << ") idx=" << idx << std::endl;
            }
        }
    }
}

int16_t SceneManager::GetEntrance(int x, int y) const {
    if (x < 0 || x >= m_worldMapWidth || y < 0 || y >= m_worldMapHeight) return -1;
    size_t idx = static_cast<size_t>(x) * m_worldMapWidth + y;
    if (idx >= m_worldEntrance.size()) return -1;
    return m_worldEntrance[idx];
}

int16_t SceneManager::GetEventData(int sceneId, int eventId, int index) const {
    if (sceneId < 0 || sceneId >= m_eventData.size()) return 0;
    if (eventId < 0 || eventId >= 200) return 0;
    if (index < 0 || index >= 11) return 0;
    return m_eventData[sceneId].data[eventId][index];
}

void SceneManager::SetEventData(int sceneId, int eventId, int index, int16_t value) {
    if (sceneId < 0 || sceneId >= m_eventData.size()) return;
    if (eventId < 0 || eventId >= 200) return;
    if (index < 0 || index >= 11) return;
    m_eventData[sceneId].data[eventId][index] = value;
}

void SceneManager::GetPositionOnScreen(int mapX, int mapY, int centerX, int centerY, int& outX, int& outY) {
    // Standard screen center
    const int SCREEN_CENTER_X = 320; 
    const int SCREEN_CENTER_Y = 240; 
    
    outX = -(mapX - centerX) * 18 + (mapY - centerY) * 18 + SCREEN_CENTER_X;
    outY = (mapX - centerX) * 9 + (mapY - centerY) * 9 + SCREEN_CENTER_Y;
}

bool SceneManager::CanWalk(int x, int y) {
    if (x < 0 || x >= SCENE_MAP_SIZE || y < 0 || y >= SCENE_MAP_SIZE) return false;
    
    // Layer 1 check (Building/Obstacle)
    int16_t tile1 = GetSceneTile(m_currentSceneId, 1, x, y);
    
    // Logic from KYS:
    // If tile1 > 0, it's an object/building.
    // However, some objects are walkable?
    // In original code, it checks if tile1 is 0.
    // If tile1 != 0, it is blocked.
    // UNLESS it's a special "overhead" tile which is drawn but walkable?
    // But usually Layer 1 is collision layer.
    
    if (tile1 != 0) return false;

    // Layer 3 check (Events)
    int16_t eventId = GetSceneTile(m_currentSceneId, 3, x, y);
    if (eventId > 0) {
        int16_t pic = GetEventData(m_currentSceneId, eventId, 5);
        // If event has a picture, it is blocking (NPC, Chest, Object)
        // If event is invisible (pic == 0), it is walkable (Trigger area)
        if (pic != 0) return false;
    }
    
    // Layer 0 (Ground) checks
    int16_t tile0 = GetSceneTile(m_currentSceneId, 0, x, y);
    // Water checks (simplified)
    // 358-362, 522, 1022, 1324-1330, 1348 are water/unwalkable
    if ((tile0 >= 358 && tile0 <= 362) || tile0 == 522 || tile0 == 1022 || 
        (tile0 >= 1324 && tile0 <= 1330) || tile0 == 1348) {
        return false;
    }
    
    return true;
}

void SceneManager::Update(uint32_t ticks) {
    // Water effect: Change palette every 200ms
    if (ticks - m_lastWaterUpdate > 200) {
        GraphicsUtils::ChangeCol(ticks);
        m_lastWaterUpdate = ticks;
    }
    
    // Cloud movement: every 40ms
    if (ticks - m_lastCloudUpdate > 40) {
        if (m_clouds.empty()) {
            m_clouds.resize(20); 
            for (auto& cloud : m_clouds) {
                cloud.x = rand() % 17280;
                cloud.y = rand() % 8640;
                cloud.speedX = 1 + rand() % 3;
                cloud.speedY = 0;
                cloud.picNum = rand() % 9;
                cloud.alpha = 25 + rand() % 50;
                cloud.shadow = 0;
            }
        }
        
        for (auto& cloud : m_clouds) {
            cloud.x += cloud.speedX;
            cloud.y += cloud.speedY;
            
            if (cloud.x > 17279) cloud.x = 0;
            if (cloud.y > 8639) cloud.y = 0;
        }
        
        m_lastCloudUpdate = ticks;
    }
}

void SceneManager::UpdateEventPosition(int sceneId, int eventId, int oldX, int oldY, int newX, int newY) {
    // Check if the old position actually contains this event
    // This prevents clearing another event if multiple events overlap (though unlikely in Layer 3)
    if (GetSceneTile(sceneId, 3, oldX, oldY) == eventId) {
        SetSceneTile(sceneId, 3, oldX, oldY, -1); // -1 = No Event
    }
    
    // Set new position
    if (newX >= 0 && newY >= 0) {
        SetSceneTile(sceneId, 3, newX, newY, eventId);
    }
}

void SceneManager::RefreshEventLayer(int sceneId) {
    if (sceneId < 0 || (size_t)sceneId >= m_eventData.size()) {
        std::cerr << "[RefreshEventLayer] Invalid sceneId or eventData empty. Id: " << sceneId << " Size: " << m_eventData.size() << std::endl;
        return;
    }

    std::cout << "[RefreshEventLayer] Refreshing Scene " << sceneId << "..." << std::endl;
    int count = 0;

    // 1. 清空该场景的 Layer 3 (事件层)
    for (int x = 0; x < SCENE_MAP_SIZE; ++x) {
        for (int y = 0; y < SCENE_MAP_SIZE; ++y) {
            SetSceneTile(sceneId, 3, x, y, -1); // -1 表示该位置无事件
        }
    }

    // 2. 根据 DData (m_eventData) 重新填充 Layer 3
    for (int e = 0; e < 200; ++e) {
        // DData 结构: Index 10 is X, Index 9 is Y
        int16_t x = GetEventData(sceneId, e, 10);
        int16_t y = GetEventData(sceneId, e, 9);
        
        if (x > 0 && x < SCENE_MAP_SIZE && y >= 0 && y < SCENE_MAP_SIZE) {
            SetSceneTile(sceneId, 3, x, y, (int16_t)e);
            count++;
        }
    }
    std::cout << "[RefreshEventLayer] Refreshed " << count << " events." << std::endl;
}

void SceneManager::DrawClouds(SDL_Renderer* renderer, int centerX, int centerY) {
    if (m_cloudPicData.empty()) return;
    
    SDL_Surface* screen = GameManager::getInstance().getScreenSurface();
    if (!screen) return;
    
    // Draw clouds based on their position
    // Simple wrapping logic for screen coverage
    for (const auto& cloud : m_clouds) {
        // Map large world coords to screen
        // Just for visual effect:
        int sx = (cloud.x / 10) % (640 + 200) - 100;
        int sy = (cloud.y / 10) % (480 + 200) - 100;
        
        // Draw if within screen bounds (roughly)
        if (sx > -200 && sx < 640 && sy > -200 && sy < 480) {
            // Draw using RLE
            // Note: This draws opaque clouds. Alpha blending would require a custom blender.
            // For now, satisfy the "draw" requirement.
            if (cloud.picNum >= 0 && cloud.picNum < m_cloudIdxData.size()) {
                int offset = m_cloudIdxData[cloud.picNum];
                if (offset > 0 && offset < m_cloudPicData.size()) {
                     GraphicsUtils::DrawRLE8(screen, sx, sy, &m_cloudPicData[offset], m_cloudPicData.size() - offset);
                }
            }
        }
    }
}

void SceneManager::DrawWorldMap(SDL_Renderer* renderer, int centerX, int centerY) {
    if (m_mmpIdxData.empty() || m_mmpPicData.empty() || m_worldEarth.empty()) {
        static bool loggedEmpty = false;
        if (!loggedEmpty) {
            std::cerr << "[DrawWorldMap] MaxMap or World resources empty! "
                      << "Idx: " << m_mmpIdxData.size()
                      << " Pic: " << m_mmpPicData.size()
                      << " Earth: " << m_worldEarth.size() << std::endl;
            loggedEmpty = true;
        }
        return;
    }

    // Draw Map
    int range = 20;

    // Draw Map Layers
    // Order: Earth -> Surface -> Building -> Player -> Clouds
    // Note: Earth/Surface should be drawn Back-to-Front (Small sum to Large sum) just like Buildings
    // to ensure proper occlusion if Surface contains standing objects.

    // 1. Draw Earth and Surface
    for (int sum = -29; sum <= 41; ++sum) {
        for (int i = -16; i <= 16; ++i) {
            int i1 = centerX + i + (sum / 2);
            int i2 = centerY - i + (sum - sum / 2);

            if (i1 < 0 || i1 >= m_worldMapWidth || i2 < 0 || i2 >= m_worldMapHeight) continue;
            // Visible range check (roughly match building loop range for consistency)
            // if (!(sum >= -29 && sum <= 41 && i >= -16 && i <= 16)) continue; 

            int idx = i1 * m_worldMapWidth + i2;
            // Check bounds just in case
            if (idx < 0 || idx >= (int)m_worldEarth.size()) continue;

            int sx, sy;
            GetPositionOnScreen(i1, i2, centerX, centerY, sx, sy);

            // Draw Earth (Ground)
            int16_t eTile = m_worldEarth[idx];
            {
                int picNum = eTile / 2;
                int offset = 0;
                if (picNum > 0) {
                    int idxIndex = picNum - 1;
                    if (idxIndex >= 0 && idxIndex < (int)m_mmpIdxData.size()) {
                        offset = m_mmpIdxData[idxIndex];
                    } else {
                        offset = -1;
                    }
                }
                if (offset >= 0 && offset < (int)m_mmpPicData.size()) {
                    GraphicsUtils::DrawRLE8(GameManager::getInstance().getScreenSurface(), sx, sy,
                                            &m_mmpPicData[offset], m_mmpPicData.size() - offset);
                }
            }

            // Draw Surface (Decor/Roads/Trees)
            if (!m_worldSurface.empty()) {
                int16_t sTile = m_worldSurface[idx];
                if (sTile > 0) {
                    int picNum = sTile / 2;
                    int idxIndex = picNum - 1;
                    if (idxIndex >= 0 && idxIndex < (int)m_mmpIdxData.size()) {
                        int offset = m_mmpIdxData[idxIndex];
                        if (offset >= 0 && offset < (int)m_mmpPicData.size()) {
                            GraphicsUtils::DrawRLE8(GameManager::getInstance().getScreenSurface(), sx, sy,
                                                    &m_mmpPicData[offset], m_mmpPicData.size() - offset);
                        }
                    }
                }
            }
        }
    }

    // 2. Draw Buildings (Sorted)
    struct BuildingPos {
        int mapX;
        int mapY;
        int16_t pic;
        int16_t width;
    };
    struct CenterPos {
        int cx2;
        int cy2;
    };

    BuildingPos buildingList[1200];
    CenterPos centerList[1200];
    int buildingCount = 0;

    for (int sum = -29; sum <= 41; ++sum) {
        for (int i = -16; i <= 16; ++i) {
            int i1 = centerX + i + (sum / 2);
            int i2 = centerY - i + (sum - sum / 2);

            if (i1 < 0 || i1 >= m_worldMapWidth || i2 < 0 || i2 >= m_worldMapHeight) {
                continue;
            }

            int idx = i1 * m_worldMapWidth + i2;
            if (idx < 0 || idx >= (int)m_worldBuilding.size()) continue;

            int16_t tempPic = 0;
            if (!m_worldBuilding.empty()) {
                tempPic = m_worldBuilding[idx];
            }
            
            // Check Referenced Building Layer (BuildX)
            if (tempPic == 0 && !m_worldBuildX.empty()) {
                int16_t sceneId = m_worldBuildX[idx];
                if (sceneId > 0 && sceneId < (int)m_scenes.size()) {
                    int16_t mapNum = m_scenes[sceneId].getMapNum();
                    if (mapNum > 0) {
                        tempPic = mapNum * 2; 
                    }
                }
            }

            int px, py;
            GetPositionOnScreen(i1, i2, centerX, centerY, px, py);

            if (tempPic > 0) {
                int picNum = tempPic / 2;
                int idxIndex = picNum - 1;
                if (idxIndex >= 0 && idxIndex < (int)m_mmpIdxData.size() && buildingCount < 1200) {
                    int offset = m_mmpIdxData[idxIndex];
                    if (offset >= 0 && offset < (int)m_mmpPicData.size()) {
                        int16_t width = 36;
                        if (offset + 2 <= (int)m_mmpPicData.size()) {
                            width = static_cast<int16_t>(m_mmpPicData[offset] | (m_mmpPicData[offset + 1] << 8));
                        }
                        buildingList[buildingCount].mapX = i1;
                        buildingList[buildingCount].mapY = i2;
                        buildingList[buildingCount].pic = tempPic;
                        buildingList[buildingCount].width = width;
                        centerList[buildingCount].cx2 = i1 * 2 - (width + 35) / 36 + 1;
                        centerList[buildingCount].cy2 = i2 * 2 - (width + 35) / 36 + 1;
                        buildingCount++;
                    }
                }
            }
        }
    }

    for (int i1 = 0; i1 < buildingCount - 1; ++i1) {
        for (int i2 = i1 + 1; i2 < buildingCount; ++i2) {
            int s1 = centerList[i1].cx2 + centerList[i1].cy2;
            int s2 = centerList[i2].cx2 + centerList[i2].cy2;
            if (s1 > s2) {
                BuildingPos bp = buildingList[i1];
                buildingList[i1] = buildingList[i2];
                buildingList[i2] = bp;
                CenterPos cp = centerList[i1];
                centerList[i1] = centerList[i2];
                centerList[i2] = cp;
            }
        }
    }

    for (int idx = 0; idx < buildingCount; ++idx) {
        int x = buildingList[idx].mapX;
        int y = buildingList[idx].mapY;
        int16_t picVal = buildingList[idx].pic;
        int sx, sy;
        GetPositionOnScreen(x, y, centerX, centerY, sx, sy);
        int picNum = picVal / 2;
        int idxIndex = picNum - 1;
        if (idxIndex >= 0 && idxIndex < (int)m_mmpIdxData.size()) {
            int offset = m_mmpIdxData[idxIndex];
            if (offset >= 0 && offset < (int)m_mmpPicData.size()) {
                GraphicsUtils::DrawRLE8(GameManager::getInstance().getScreenSurface(), sx, sy,
                                        &m_mmpPicData[offset], m_mmpPicData.size() - offset);
            }
        }
    }

    
    // Draw Player
    int screenX, screenY;
    GetPositionOnScreen(centerX, centerY, centerX, centerY, screenX, screenY);

    int face = GameManager::getInstance().getMainMapFace();
    int spriteFace = 0;
    switch(face) {
         case 0: spriteFace = 3; break;
         case 1: spriteFace = 0; break;
         case 2: spriteFace = 2; break;
         case 3: spriteFace = 1; break;
    }
    int frame = GameManager::getInstance().getWalkFrame();
    if (GameManager::getInstance().getInShip() == 1) {
        int shipFrame = (frame + 1) / 2;
        int shipPic = 3714 + spriteFace * 4 + shipFrame;
        DrawMmapSprite(renderer, shipPic, screenX, screenY, 0);
    } else {
        int playerPic = 2501 + spriteFace * 7 + frame;
        DrawSmpSprite(renderer, playerPic, screenX, screenY, 0);
    }
    
    // Draw Clouds
    DrawClouds(renderer, centerX, centerY);
}

void SceneManager::DrawScene(SDL_Renderer* renderer, int centerX, int centerY) {
    // If Scene is -1 (World Map), delegate
    if (m_currentSceneId == -1) {
        DrawWorldMap(renderer, centerX, centerY);
        return;
    }

    // Safety check: if resources failed to load, don't draw
    if (m_smpIdxData.empty() || m_smpPicData.empty()) {
        static bool logged = false;
        if (!logged) {
            std::cerr << "[DrawScene] CRITICAL: SceneMap resources (smp/sdx) are empty! Cannot draw scene." << std::endl;
            logged = true;
        }
        return;
    }
    
    // Check if Map Data is loaded
    if (m_mapData.empty()) {
        static bool loggedMap = false;
        if (!loggedMap) {
            std::cerr << "[DrawScene] CRITICAL: Map Data (allsin.grp) is empty! Cannot draw scene." << std::endl;
            loggedMap = true;
        }
        return; // Nothing to draw
    }

    // Optimization: Pre-calculate Sprite Map
    // Map: [x][y] -> Sprite Index (Pic)
    // Initialize with -1 (No Sprite)
    // Using a flat vector or stack array? 64x64 is small (4KB).
    static int16_t spriteMap[SCENE_MAP_SIZE][SCENE_MAP_SIZE];
    for(int i=0; i<SCENE_MAP_SIZE; ++i)
        for(int j=0; j<SCENE_MAP_SIZE; ++j)
            spriteMap[i][j] = -1;
    bool hidePlayer = false;
    if (m_currentSceneId >= 0 && m_currentSceneId < (int)m_eventData.size()) {
        int centerEvent = GetSceneTile(m_currentSceneId, 3, centerX, centerY);
        int16_t pic0 = m_eventData[m_currentSceneId].data[0][5];
        if (centerEvent == 0 && pic0 != 0) hidePlayer = true;
    }

    int px, py;
    GetPositionOnScreen(centerX, centerY, centerX, centerY, px, py);
    
    // Draw tiles
    // Pascal draws layer 0, 1, 2, then Events (Layer 3 refs), then Roles.
    // Actually DrawRoleOnScene draws everything in a specific order per tile.
    
    // Iterate Screen (or Map Range)
    // Pascal iterates 64x64 (Full Map) in DrawRoleOnScene?
    // No, it iterates visible range implicitly or explicit full loop?
    // Pascal: for i1 := 0 to 63 do for i2 := 0 to 63 do
    // It iterates the WHOLE 64x64 map! (Small enough).
    
    for (int i = 0; i < SCENE_MAP_SIZE; ++i) { // X (or Y? Pascal: i1)
        for (int j = 0; j < SCENE_MAP_SIZE; ++j) { // Y (or X? Pascal: i2)
            // i, j are Map Coordinates
            int sx, sy;
            GetPositionOnScreen(i, j, centerX, centerY, sx, sy);
            
            // Check if visible (roughly)
            if (sx < -100 || sx > 640 + 100 || sy < -100 || sy > 480 + 100) continue;

            // Layer 0 (Ground)
            int16_t t0 = GetSceneTile(m_currentSceneId, 0, i, j);
            // Pascal: if SData > 0 then DrawSPic... else if < 0 DrawSNewPic...
            // For now assume > 0 (Standard)
            if (t0 != 0) {
                 if (t0 > 0) DrawSmpSprite(renderer, t0 / 2 - 1, sx, sy);
                 else DrawScenePicSprite(renderer, -t0 / 2 - 1, sx, sy);
            }

            // Layer 1 (Surface/Object)
            int16_t t1 = GetSceneTile(m_currentSceneId, 1, i, j);
            int16_t h1 = GetSceneTile(m_currentSceneId, 4, i, j); // Height
            if (t1 != 0) {
                 if (t1 > 0) DrawSmpSprite(renderer, t1 / 2 - 1, sx, sy - h1);
                 else DrawScenePicSprite(renderer, -t1 / 2 - 1, sx, sy - h1);
            }
            
            // Layer 2 (Above)
            int16_t t2 = GetSceneTile(m_currentSceneId, 2, i, j);
            int16_t h2 = GetSceneTile(m_currentSceneId, 5, i, j); // Height 2? Or same height?
            // Pascal uses SData[..., 5, ...] for Layer 2 height offset
            if (t2 != 0) {
                 if (t2 > 0) DrawSmpSprite(renderer, t2 / 2 - 1, sx, sy - h2);
                 else DrawScenePicSprite(renderer, -t2 / 2 - 1, sx, sy - h2);
            }
            
            // Layer 3 (Events)
            // SData[..., 3, i, j] contains Event Index
            int16_t eventIdx = GetSceneTile(m_currentSceneId, 3, i, j);
            if (eventIdx >= 0) {
                // Look up DData
                int16_t pic = GetEventData(m_currentSceneId, eventIdx, 5);
                if (pic != 0) {
                     int16_t h = GetSceneTile(m_currentSceneId, 4, i, j); // Events usually stand on ground/object height
                     if (pic > 0) {
                         DrawSmpSprite(renderer, pic / 2 - 1, sx, sy - h);
                     } else {
                         DrawScenePicSprite(renderer, -pic / 2 - 1, sx, sy - h);
                     }
                }
            }
            
            // Draw Player (if at this tile)
            if (!hidePlayer && i == centerX && j == centerY) {
                int face = GameManager::getInstance().getMainMapFace();
                int step = GameManager::getInstance().getWalkFrame();
                // Face: 0,1,2,3 -> Pascal mapping might differ
                // Pascal: 2501 + SFace * 7 + SStep
                // SFace: 0,1,2,3?
                int playerPic = 2501 + face * 7 + step;
                int16_t h = GetSceneTile(m_currentSceneId, 4, i, j);
                DrawSmpSprite(renderer, playerPic, sx, sy - h); // 2501 is index? No, 2501 is PicNum.
                // Wait, DrawSmpSprite takes INDEX.
                // Pascal: DrawSPic(2501...).
                // If 2501 is PicNum, and user said "index = pic/2 - 1".
                // Does 2501 follow this? 2501 is odd. 2501 div 2 = 1250.
                // So index = 1249?
                // Or is 2501 ALREADY an index for DrawSPic?
                // Pascal DrawMMap: temp := 2501...; temp := temp * 2;
                // So 2501 IS the value before doubling.
                // So effectively, PicNum = 2501 * 2 = 5002.
                // DData stores 5002.
                // So DData/2 = 2501.
                // So if DrawSmpSprite takes "Index", is it 2501?
                // My LoadResources log: "Player Offset (2501): ..."
                // This implies 2501 is a direct index into the offset array (sdx).
                // So for Player, we use 2501 directly.
                // BUT for DData, DData stores Pic*2.
                // So we do (DDataVal / 2) - 1.
                // Example: DData has 5002. 5002/2 = 2501. 2501 - 1 = 2500.
                // Wait, if 2501 is the player sprite, and I pass 2500, I get the wrong one?
                // User said: "pic/2-1".
                // If 2501 is the correct index, then (5002 / 2) - 1 = 2500.
                // Maybe Player starts at 2500?
                // Let's assume the user rule "pic/2-1" applies to DData values.
                // For the Player calculation `2501 + ...`, this is already an "Index" (or PicNum).
                // If I use `DrawSmpSprite(2501...)` it assumes 2501 is the index.
                // Let's check `DrawSmpSprite` implementation.
            }
        }
    }
    GameManager::getInstance().getMainMapPosition(px, py);
    if (!hidePlayer && px >= 0 && px < SCENE_MAP_SIZE && py >= 0 && py < SCENE_MAP_SIZE) {
        // Player sprite index
        // Base: 2501 (Protagonist)
        // Face Mapping:
        // 0 = South (Down) -> 0
        // 1 = North (Up) -> 2
        // 2 = West (Left) -> 3
        // 3 = East (Right) -> 1
        
        int face = GameManager::getInstance().getMainMapFace();
        int spriteFace = 0;
        // KYS ISO View Mapping:
         // 0 = South (Down) -> 0
         // 1 = North (Up) -> 1
         // 2 = West (Left) -> 2
         // 3 = East (Right) -> 3
         
         // Final Fine-Tuning based on feedback:
         // Left (2) -> Sprite 2 (Correct)
         // Down (0) -> Sprite 3 (Correct)
         // Right (3) -> Sprite 0 (Wrong)
         // Up (1) -> Sprite 1 (Wrong)
         
         // If Right(3) is not 0, and can't be 2 or 3 (taken).
         // Then Right(3) MUST be Sprite 1.
         
         // If Up(1) is not 1, and can't be 2 or 3 (taken).
         // Then Up(1) MUST be Sprite 0.
         
         // So the mapping must be:
         // Down (0) -> Sprite 3
         // Up (1) -> Sprite 0
         // Left (2) -> Sprite 2
         // Right (3) -> Sprite 1
         
         switch(face) {
             case 0: spriteFace = 3; break; // Down -> Sprite 3
             case 1: spriteFace = 0; break; // Up -> Sprite 0
             case 2: spriteFace = 2; break; // Left -> Sprite 2
             case 3: spriteFace = 1; break; // Right -> Sprite 1
         }
         
         int frame = GameManager::getInstance().getWalkFrame();
        
        // Pic Calculation: Base + Face * 7 + Frame
        int playerPic = 2501 + spriteFace * 7 + frame;
        
        spriteMap[px][py] = playerPic; 
    }

    // Render back-to-front
    static int eventLayerCount = 0; // Use static to prevent loop, but reset occasionally?
    // Actually, just log once per scene load or something.
    // Or just comment it out now that we confirmed Event 3 is found but Pic is 0.
    
    for (int i1 = 0; i1 < SCENE_MAP_SIZE; ++i1) {
        for (int i2 = 0; i2 < SCENE_MAP_SIZE; ++i2) {
            int x, y;
            GetPositionOnScreen(i1, i2, centerX, centerY, x, y);
            
            // Culling
            if (x < -200 || x > 840 || y < -200 || y > 680) continue; 

            // Layer 0: Ground
            int16_t tile0 = GetSceneTile(m_currentSceneId, 0, i1, i2);
            if (tile0 > 0) {
                // Fix: Tile index off by one (user feedback)
                // (tile / 2) - 1 maps 2 -> 0, 4 -> 1, etc.
                DrawTile(renderer, (tile0 / 2) - 1, x, y, 0, 0);
            }
            
            // Layer 1: Building
            int16_t tile1 = GetSceneTile(m_currentSceneId, 1, i1, i2);
            int16_t height1 = GetSceneTile(m_currentSceneId, 4, i1, i2);
            if (tile1 > 0) {
                DrawTile(renderer, (tile1 / 2) - 1, x, y - height1, 0, 0);
            }
            
            // Layer 2: Decor
            int16_t tile2 = GetSceneTile(m_currentSceneId, 2, i1, i2);
            int16_t height2 = GetSceneTile(m_currentSceneId, 5, i1, i2);
            if (tile2 > 0) {
                DrawTile(renderer, (tile2 / 2) - 1, x, y - height2, 0, 0);
            }
            
            // Layer 3: Event
            int16_t tile3 = GetSceneTile(m_currentSceneId, 3, i1, i2);
            
            if (tile3 >= 0 && tile3 < 200) { // tile3 is Event ID (0..199)
                // tile3 is eventIndex. Get EventData to find pic.
                // KYS Event Data: Index 5 is Pic
                int16_t eventPic = GetEventData(m_currentSceneId, tile3, 5);
                int16_t defaultPic = GetEventData(m_currentSceneId, tile3, 7);
                if (defaultPic != 0 && eventPic != defaultPic) {
                    SetEventData(m_currentSceneId, tile3, 5, defaultPic);
                    eventPic = defaultPic;
                }
                
                if (eventPic != 0) { // Pascal logic: if <> 0 then draw
                    int drawY = y - height1;
                    if (eventPic > 0) {
                        DrawSmpSprite(renderer, (eventPic / 2) - 1, x, drawY, 0);
                    } else {
                        DrawMmapSprite(renderer, (-eventPic / 2) - 1, x, drawY, 0);
                    }
                }
            }

            // Sprites (Player & dynamic sprites from pre-calculated map)
            if (spriteMap[i1][i2] != -1) {
                // Adjust for height too for Player
                int drawY = y - height1;
                DrawSprite(renderer, spriteMap[i1][i2], x, drawY, 0);
            }
        }
    }
    
    // Draw Clouds over the map
    DrawClouds(renderer, centerX, centerY);
}

// Global Scale for Characters
static const float CHAR_SCALE = 1.15f;

void SceneManager::DrawTile(SDL_Renderer* renderer, int picIndex, int x, int y, int offX, int offY) {
    if (picIndex < 0 || picIndex >= m_smpIdxData.size()) return;
    
    int offset = m_smpIdxData[picIndex];
    if (offset < 0 || offset >= m_smpPicData.size()) return;
    
    SDL_Surface* screen = GameManager::getInstance().getScreenSurface();
    if (!screen) return;
    
    GraphicsUtils::DrawRLE8(screen, x, y, &m_smpPicData[offset], m_smpPicData.size() - offset);
}

void SceneManager::DrawTileShadow(SDL_Renderer* renderer, int picIndex, int x, int y, int offX, int offY, int shadow) {
    if (picIndex < 0 || picIndex >= m_smpIdxData.size()) return;
    if (m_smpPicData.empty() || m_smpIdxData.empty()) return;
    int offset = m_smpIdxData[picIndex];
    if (offset < 0 || offset >= m_smpPicData.size()) return;
    SDL_Surface* screen = GameManager::getInstance().getScreenSurface();
    if (!screen) return;
    GraphicsUtils::DrawRLE8(screen, x, y, &m_smpPicData[offset], m_smpPicData.size() - offset, shadow);
}

void SceneManager::DrawWarTile(SDL_Renderer* renderer, int picIndex, int x, int y, int offX, int offY, int green, int red, int gray, int white) {
    if (picIndex < 0 || picIndex >= m_wmpIdxData.size()) return;
    if (m_wmpPicData.empty() || m_wmpIdxData.empty()) return;
    int offset = 0;
    if (picIndex > 0) {
        int idxIndex = picIndex - 1;
        if (idxIndex < 0 || idxIndex >= (int)m_wmpIdxData.size()) return;
        offset = m_wmpIdxData[idxIndex];
    }
    if (offset < 0 || offset >= m_wmpPicData.size()) return;
    
    SDL_Surface* screen = GameManager::getInstance().getScreenSurface();
    if (!screen) return;
    
    GraphicsUtils::DrawRLE8(screen, x, y, &m_wmpPicData[offset], m_wmpPicData.size() - offset, 0, 1.0f, green, red, gray, white);
}

void SceneManager::DrawSmpSprite(SDL_Renderer* renderer, int picIndex, int x, int y, int frame) {
    if (m_smpPicData.empty() || m_smpIdxData.empty()) return;
    
    // Debug: Check bounds
    if (picIndex < 0 || picIndex >= (int)m_smpIdxData.size()) {
        static bool loggedBounds = false;
        if (!loggedBounds) {
            std::cerr << "[DrawSmpSprite] Index out of bounds: " << picIndex << " Size: " << m_smpIdxData.size() << std::endl;
            loggedBounds = true;
        }
        return;
    }
    
    int offset = m_smpIdxData[picIndex];
    if (offset < 0 || offset >= (int)m_smpPicData.size()) {
        // Offset 0 might be valid if the first sprite starts at 0
        return;
    }
    
    SDL_Surface* screen = GameManager::getInstance().getScreenSurface();
    if (!screen) return;
    
    // Debug: Log successful draw attempt for specific indices
    // if (picIndex == 4141 || picIndex == 12) { // 8284/2-1 or 27/2-1
    //    std::cout << "[DrawSmpSprite] Drawing Pic " << picIndex << " at " << x << "," << y << " Offset: " << offset << std::endl;
    // }
    
    GraphicsUtils::DrawRLE8(screen, x, y, &m_smpPicData[offset], m_smpPicData.size() - offset, 0, CHAR_SCALE);
}

void SceneManager::DrawMmapSprite(SDL_Renderer* renderer, int picIndex, int x, int y, int frame) {
    if (m_mmpPicData.empty() || m_mmpIdxData.empty()) return;
    if (picIndex < 0 || picIndex >= (int)m_mmpIdxData.size()) return;
    
    int offset = m_mmpIdxData[picIndex];
    if (offset < 0 || offset >= (int)m_mmpPicData.size()) return;
    
    SDL_Surface* screen = GameManager::getInstance().getScreenSurface();
    if (!screen) return;
    
    GraphicsUtils::DrawRLE8(screen, x, y, &m_mmpPicData[offset], m_mmpPicData.size() - offset, 0, CHAR_SCALE);
}

void SceneManager::DrawScenePicSprite(SDL_Renderer* renderer, int picIndex, int x, int y, int frame) {
    if (picIndex < 0 || picIndex >= (int)m_scenePics.size()) return;
    
    const auto& sp = m_scenePics[picIndex];
    if (!sp.surface) return;
    
    SDL_Surface* screen = GameManager::getInstance().getScreenSurface();
    if (!screen) return;
    
    // PNGs in Scene.Pic already have their own offset (sp.x, sp.y)
    // In Pascal: x1 := px - Scenepic[num].x + 1;
    SDL_Rect dest = { x - sp.x, y - sp.y, sp.surface->w, sp.surface->h };
    
    // Scale if needed
    if (CHAR_SCALE != 1.0f) {
        dest.w = (int)(dest.w * CHAR_SCALE);
        dest.h = (int)(dest.h * CHAR_SCALE);
    }
    
    SDL_BlitSurface(sp.surface, NULL, screen, &dest);
}

void SceneManager::DrawSprite(SDL_Renderer* renderer, int picIndex, int x, int y, int frame) {
    // This is now a dispatcher for generic calls (like player drawing)
    // For now, assume player/protagonist uses smp (matching current SceneManager::DrawScene logic)
    // but in a real KYS game, protagonist is usually in mmap.grp.
    // However, the current code (line 407) calculates playerPic as 2501 + ..., 
    // and 2501 is typically an smp index.
    DrawSmpSprite(renderer, picIndex, x, y, frame);
}


