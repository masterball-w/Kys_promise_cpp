#pragma once
#include <vector>
#include <string>
#include <SDL3/SDL.h>
#include "Scene.h"
#include "GameTypes.h" // Assuming this exists or I should create it for common types

// Constants
constexpr int MAX_SCENES = 100; // Adjust as needed
constexpr int SCENE_MAP_SIZE = 64;
constexpr int SCENE_LAYERS = 6;

class SceneManager {
public:
    static SceneManager& getInstance();

    bool Init();
    bool LoadEventData(const std::string& path); // Loads Event Data (DData) from alldef.grp
    bool LoadMapData(const std::string& path); // Loads Map data (SData) from allsin.grp
    
    // Save Data
    bool SaveEventData(const std::string& path);
    bool SaveMapData(const std::string& path);

    // Set Scenes (loaded from ranger.grp/save file)
    void SetScenes(const std::vector<Scene>& scenes);
    
    // Core drawing function
    // centerX, centerY: Tile coordinates of the camera center (player position)
    void DrawScene(SDL_Renderer* renderer, int centerX, int centerY);
    
    // Helpers
    void SetCurrentScene(int sceneId);
    int GetCurrentSceneId() const { return m_currentSceneId; }
    
    // Movement & Collision
    bool CanWalk(int x, int y);
    void GetPositionOnScreen(int mapX, int mapY, int centerX, int centerY, int& outX, int& outY);
    
    // Animation & Effects
    void Update(uint32_t ticks); // Update animations (clouds, water, etc.)
    void DrawClouds(SDL_Renderer* renderer, int centerX, int centerY);
    void SetSceneWeather(int mapMode);
    int GetSceneWeather() const { return m_sceneWeatherMode; }
    
    // Accessors for SData equivalent
    int16_t GetSceneTile(int sceneId, int layer, int x, int y) const;
    void SetSceneTile(int sceneId, int layer, int x, int y, int16_t value);
    
    // Update Event Position in Map (Layer 3)
    void UpdateEventPosition(int sceneId, int eventId, int oldX, int oldY, int newX, int newY);

    // Accessors for DData (Event Definitions)
    // Pascal: DData[SceneId, EventId, Index]
    // EventId: 0..199, Index: 0..10
    int16_t GetEventData(int sceneId, int eventId, int index) const;
    void SetEventData(int sceneId, int eventId, int index, int16_t value);

    // Accessors for Scene definitions
    Scene* GetScene(int sceneId);
    size_t GetSceneCount() const { return m_scenes.size(); }
    const std::vector<Scene>& getScenes() const { return m_scenes; }
    size_t GetMapSceneCount() const;
    size_t GetEventSceneCount() const { return m_eventData.size(); }
    
    // 刷新事件层 (根据 DData 同步 SData 的 Layer 3)
    void RefreshEventLayer(int sceneId);

    /// Pascal InitialScene: reset DData[5]:=DData[7], sync SData layer 3.
    /// (No SceneImg bake — C++ redraws from DData/SData each frame.)
    void InitialScene();

    /// Pascal UpdateScene — C++ 每帧重绘 SceneImg 缓存未使用，保留调用点以对齐 instruct_3 时序
    void UpdateSceneGraphic(int mapX, int mapY, int oldPic, int newPic);
    
    // 加载资源 (贴图等)
    bool LoadResources();
    
    // Testing Helper
    void CreateMockScene(int id);

    // Helper to draw a single tile (from smp)
    void DrawTile(SDL_Renderer* renderer, int picIndex, int x, int y, int offX, int offY);
    void DrawTileShadow(SDL_Renderer* renderer, int picIndex, int x, int y, int offX, int offY, int shadow);
    void DrawWarTile(SDL_Renderer* renderer, int picIndex, int x, int y, int offX, int offY, int green = 0, int red = 0, int gray = 0, int white = 0);
    
    // Helper to draw a sprite from smp (static objects)
    void DrawSmpSprite(SDL_Renderer* renderer, int picIndex, int x, int y, int frame = 0);

    // Helper to draw a sprite from mmap.grp (roles/NPCs)
    void DrawMmapSprite(SDL_Renderer* renderer, int picIndex, int x, int y, int frame = 0);

    // 绘制精灵 (来自 Scene.Pic, 动态物体)
    void DrawScenePicSprite(SDL_Renderer* renderer, int picIndex, int x, int y, int frame = 0, float scale = -1.0f);

    // 通用精灵绘制 (根据 picIndex 自动判断来源)
    void DrawSprite(SDL_Renderer* renderer, int picIndex, int x, int y, int frame = 0);

    // 大地图辅助查询
    int16_t GetWorldEarth(int x, int y) const;
    int16_t GetWorldSurface(int x, int y) const;
    int16_t GetWorldBuildX(int x, int y) const;
    int16_t GetWorldBuildY(int x, int y) const;

    // 场景入口映射 (大地图 480x480 -> 场景号)，对应 Pascal 的 Entrance 数组
    void ResetEntrance();
    int16_t GetEntrance(int x, int y) const;

private:
    SceneManager();
    ~SceneManager() = default;
    SceneManager(const SceneManager&) = delete;
    SceneManager& operator=(const SceneManager&) = delete;

    // 大地图数据 (World Map) - 对应 Pascal 中的
    // earth, surface, building, buildx, buildy 五个数组
    // 坐标范围 0..479 x 0..479, 存储方式: index = x * 480 + y
    std::vector<int16_t> m_worldEarth;    // EARTH.002
    std::vector<int16_t> m_worldSurface;  // surface.002
    std::vector<int16_t> m_worldBuilding; // building.002
    std::vector<int16_t> m_worldBuildX;   // buildx.002
    std::vector<int16_t> m_worldBuildY;   // buildy.002
    std::vector<int16_t> m_worldEntrance; // Entrance: 世界坐标到场景编号映射
    int m_worldMapWidth = 480;
    int m_worldMapHeight = 480;
    
    // 战斗地图资源 (WarMap) - wmp/wdx
    std::vector<uint8_t> m_wmpPicData; // wmp
    std::vector<int32_t> m_wmpIdxData; // wdx

    // Helper to draw World Map
    void DrawWorldMap(SDL_Renderer* renderer, int centerX, int centerY);
    bool LoadWorldMap();

    // Scene Definitions
    std::vector<Scene> m_scenes;
    
    // Map Data: [SceneId][Layer][Y][X]
    std::vector<int16_t> m_mapData;

    // Event Data: [SceneId][EventId][Index]
    // EventId max 200, Index max 11
    struct EventData {
        int16_t data[200][11];
    };
    std::vector<EventData> m_eventData;
    
    // 场景图块资源 (SceneMap) - smp/sdx
    std::vector<uint8_t> m_smpPicData; // smp
    std::vector<int32_t> m_smpIdxData; // sdx
    
    // 大地图资源 (MaxMap) - mmap.grp/mmap.idx
    std::vector<uint8_t> m_mmpPicData; // mmp
    std::vector<int32_t> m_mmpIdxData; // midx

    // 动态场景贴图 (Scene.Pic) - PNG 格式集合
    struct ScenePic {
        int x, y, black;
        SDL_Surface* surface = nullptr;
    };
    std::vector<ScenePic> m_scenePics;

    // Cloud Data
    struct Cloud {
        int x, y;
        int speedX, speedY;
        int picNum;
        int shadow;
        int alpha;
    };
    std::vector<Cloud> m_clouds;
    std::vector<uint8_t> m_cloudPicData; // cloud.grp
    std::vector<int32_t> m_cloudIdxData; // cloud.idx
    
    int m_currentSceneId;
    int m_sceneWeatherMode = 0;
    
    // Timer state
    uint32_t m_lastWaterUpdate;
    uint32_t m_lastCloudUpdate;
};
