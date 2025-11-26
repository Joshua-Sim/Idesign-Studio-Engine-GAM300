#include "pch.h"
#include <glm/glm/glm.hpp>
#include <string>
#include "Coordinator.h"

namespace Framework {

    enum class GamePhase
    {
        GameStart,
        Day,
        Night
    };

    class PhaseManager
    {
    public:
        // 1. REMOVED: static Instance() (We are using a pointer in GameSystem now)

        // 2. CHANGED: Constructor is now PUBLIC and takes Coordinator*
        PhaseManager(Coordinator* coord) : coordinator(coord) {
            // Initialize defaults if needed
        }

        void Initialize();

        // --- Main Control ---
        void SetPhase(GamePhase newPhase);
        void TogglePhase();
        GamePhase GetCurrentPhase() const { return m_currentPhase; }

        // --- Configuration ---
        void StartGame() { SetPhase(GamePhase::Day); }
        void SetDaySpawn(const glm::vec3& pos, const glm::vec3& rot) { m_daySpawnPos = pos; m_daySpawnRot = rot; }
        void SetNightSpawn(const glm::vec3& pos, const glm::vec3& rot) { m_nightSpawnPos = pos; m_nightSpawnRot = rot; }

        glm::vec3& GetDaySpawnPosRef() { return m_daySpawnPos; }
        glm::vec3& GetNightSpawnPosRef() { return m_nightSpawnPos; }

    private:
        // 3. ADDED: Store the coordinator pointer
        Coordinator* coordinator;

        // Internal helper
        void TeleportPlayer(const glm::vec3& position, const glm::vec3& rotation);

        GamePhase m_currentPhase = GamePhase::Day;

        glm::vec3 m_daySpawnPos{ -53.965, 9.0f, 9.5f };
        glm::vec3 m_daySpawnRot{ 0.0f, 0.0f, 0.0f };

        glm::vec3 m_nightSpawnPos{ -36.0f, 9.0f, 9.5f };
        glm::vec3 m_nightSpawnRot{ 0.0f, 0.0f, 0.0f };
    };
}