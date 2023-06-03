#include "runtime/function/framework/level/level.h"

#include "runtime/core/base/macro.h"

#include "runtime/resource/asset_manager/asset_manager.h"
#include "runtime/resource/res_type/common/level.h"

#include "runtime/engine.h"
#include "runtime/function/character/character.h"
#include "runtime/function/framework/object/object.h"
#include "runtime/function/particle/particle_manager.h"
#include "runtime/function/physics/physics_manager.h"
#include "runtime/function/physics/physics_scene.h"
#include "runtime/function/render/render_system.h"
#include <limits>

namespace Piccolo
{
    void Level::clear()
    {
        m_current_active_character.reset();
        m_gobjects.clear();

        ASSERT(g_runtime_global_context.m_physics_manager);
        g_runtime_global_context.m_physics_manager->deletePhysicsScene(m_physics_scene);
    }

    GObjectID Level::createObject(const ObjectInstanceRes& object_instance_res)
    {
        GObjectID object_id = ObjectIDAllocator::alloc();
        ASSERT(object_id != k_invalid_gobject_id);

        std::shared_ptr<GObject> gobject;
        try
        {
            gobject = std::make_shared<GObject>(object_id);
        }
        catch (const std::bad_alloc&)
        {
            LOG_FATAL("cannot allocate memory for new gobject");
        }

        bool is_loaded = gobject->load(object_instance_res);
        if (is_loaded)
        {
            m_gobjects.emplace(object_id, gobject);
        }
        else
        {
            LOG_ERROR("loading object " + object_instance_res.m_name + " failed");
            return k_invalid_gobject_id;
        }
        return object_id;
    }

    bool Level::load(const std::string& level_res_url)
    {
        LOG_INFO("loading level: {}", level_res_url);

        m_level_res_url = level_res_url;

        LevelRes   level_res;
        const bool is_load_success = g_runtime_global_context.m_asset_manager->loadAsset(level_res_url, level_res);
        if (is_load_success == false)
        {
            return false;
        }

        ASSERT(g_runtime_global_context.m_physics_manager);
        m_physics_scene = g_runtime_global_context.m_physics_manager->createPhysicsScene(level_res.m_gravity);
        ParticleEmitterIDAllocator::reset();

        for (const ObjectInstanceRes& object_instance_res : level_res.m_objects)
        {
            createObject(object_instance_res);
        }

        // create active character
        for (const auto& object_pair : m_gobjects)
        {
            std::shared_ptr<GObject> object = object_pair.second;
            if (object == nullptr)
                continue;

            if (level_res.m_character_name == object->getName())
            {
                m_current_active_character = std::make_shared<Character>(object);
                break;
            }
        }

        m_is_loaded = true;

        LOG_INFO("level load succeed");

        return true;
    }

    void Level::unload()
    {
        clear();
        LOG_INFO("unload level: {}", m_level_res_url);
    }

    bool Level::save()
    {
        LOG_INFO("saving level: {}", m_level_res_url);
        LevelRes output_level_res;

        const size_t                    object_cout    = m_gobjects.size();
        std::vector<ObjectInstanceRes>& output_objects = output_level_res.m_objects;
        output_objects.resize(object_cout);

        size_t object_index = 0;
        for (const auto& id_object_pair : m_gobjects)
        {
            if (id_object_pair.second)
            {
                id_object_pair.second->save(output_objects[object_index]);
                ++object_index;
            }
        }

        const bool is_save_success =
            g_runtime_global_context.m_asset_manager->saveAsset(output_level_res, m_level_res_url);

        if (is_save_success == false)
        {
            LOG_ERROR("failed to save {}", m_level_res_url);
        }
        else
        {
            LOG_INFO("level save succeed");
        }

        return is_save_success;
    }

    void Level::generateMaze()
    {
        //
        LevelObjectsMap::iterator iter = m_gobjects.begin();
        while (iter != m_gobjects.end())
        {
            //
            RenderSwapContext& swap_context = g_runtime_global_context.m_render_system->getSwapContext();
            swap_context.getLogicSwapData().addDeleteGameObject(GameObjectDesc {iter->first, {}});
            //
            deleteGObjectByID(iter->first);
            iter = m_gobjects.begin();
        }
        //
        ObjectInstanceRes Ground;
        Ground.m_name       = "Ground";
        Ground.m_definition = "asset/objects/environment/floor/floor.object.json";
        createObject(Ground);
        //
        ObjectInstanceRes Player;
        Player.m_name       = "Player";
        Player.m_definition = "asset/objects/character/player/player.object.json";
        createObject(Player);
        //
        const int cols = 5;
        const int rows = 8;
        //
        int mazeTypes[rows][cols];
        bool mazeDoors[rows][cols][4];
        for (int i = 0; i < rows; i++)
        {
            for (int j = 0; j < cols; j++)
            {
				mazeTypes[i][j] = cols * i + j;
                for (int k = 0; k < 4; k++)
                {
					mazeDoors[i][j][k] = false;
				}
			}
		}
        for (int i = 0; i < rows; i++)
        {
            for (int j = 0; j < cols; j++)
            {
                std::vector<int> candidateDoorDir;
                //
                if (i > 0 && mazeTypes[i][j] != mazeTypes[i - 1][j])
                {
                    candidateDoorDir.push_back(0);
                }
                //
                if (j < cols - 1 && mazeTypes[i][j] != mazeTypes[i][j + 1])
                {
                    candidateDoorDir.push_back(1);
                }
                //
                if (i < rows - 1 && mazeTypes[i][j] != mazeTypes[i + 1][j])
                {
					candidateDoorDir.push_back(2);
				}
                //
                if (j > 0 && mazeTypes[i][j] != mazeTypes[i][j - 1])
                {
                    candidateDoorDir.push_back(3);
                }
                if (candidateDoorDir.size() == 0)
                {
                    break;
                }
                //
                int openDoorDir = candidateDoorDir[std::rand() % candidateDoorDir.size()];
                int newRoomID;
                //
                mazeDoors[i][j][openDoorDir] = true;
                if (openDoorDir == 0)
                {
                    mazeDoors[i - 1][j][2] = true;
                    newRoomID = mazeTypes[i - 1][j];
                }
                else if (openDoorDir == 1)
                {
					mazeDoors[i][j + 1][3] = true;
					newRoomID = mazeTypes[i][j + 1];
				}
                else if (openDoorDir == 2)
                {
					mazeDoors[i + 1][j][0] = true;
					newRoomID = mazeTypes[i + 1][j];
				}
                else if (openDoorDir == 3)
                {
					mazeDoors[i][j - 1][1] = true;
					newRoomID = mazeTypes[i][j - 1];
                }
                //
                int oldRoomID = mazeTypes[i][j];
                for (int ii = 0; ii < rows; ii++)
                {
                    for (int jj = 0; jj < cols; jj++)
                    {
                        if (mazeTypes[ii][jj] == oldRoomID)
                        {
							mazeTypes[ii][jj] = newRoomID;
						}
					}
                }
            }
        }

        //
        for (int i = 0; i < rows; i++)
        {
            for (int j = 0; j < cols; j++)
            {
                //
                if (!mazeDoors[i][j][0])
                {
                    int wallNum = i * (2 * cols + 1) + j;
                    ObjectInstanceRes Wall;
                    Wall.m_name       = "Wall_" + std::to_string(wallNum);
                    Wall.m_definition = "asset/objects/environment/wall/wall.object.json";
                    createObject(Wall);
                }
                //
                if (!mazeDoors[i][j][3])
                {
					int wallNum = i * (2 * cols + 1) + j + cols;
					ObjectInstanceRes Wall;
					Wall.m_name       = "Wall_" + std::to_string(wallNum);
					Wall.m_definition = "asset/objects/environment/wall/wall.object.json";
					createObject(Wall);
				}
                //
                if (j == cols - 1)
                {
                    int wallNum = i * (2 * cols + 1) + j + cols + 1;
                    ObjectInstanceRes Wall;
                    Wall.m_name       = "Wall_" + std::to_string(wallNum);
                    Wall.m_definition = "asset/objects/environment/wall/wall.object.json";
                    createObject(Wall);
                }
                //
                if (i == rows - 1)
                {
                    int wallNum = i * (2 * cols + 1) + j + 2 * cols + 1;
                    ObjectInstanceRes Wall;
                    Wall.m_name       = "Wall_" + std::to_string(wallNum);
                    Wall.m_definition = "asset/objects/environment/wall/wall.object.json";
                    createObject(Wall);
                }
            }
        }

        //
        for (const auto& object_pair : m_gobjects)
        {
            std::shared_ptr<GObject> object = object_pair.second;
            if (object == nullptr)
                continue;

            std::cout << object->getName();

            if ("Player" == object->getName())
            {
                m_current_active_character = std::make_shared<Character>(object);
                continue;
            }


            for (int i = 0; i < rows * (2 * cols + 1) + cols; i++)
            {
                if (("Wall_" + std::to_string(i)) == (object->getName()))
                {
                    int rowNum = int(i / (2 * cols + 1));
                    int colNum = i % (2 * cols + 1);
                    TransformComponent* transform_component = object->tryGetComponent(TransformComponent);
                    Vector3             new_translation;
                    Quaternion          new_rotation;
                    if (colNum < cols)
                    {
                        new_translation.x = -10 - 10 * (rows - 1) / 2 + rowNum * 10;
                        new_translation.y = -10 * (cols - 1) / 2 + colNum * 10;
                        new_translation.z = 0;
                    }
                    else
                    {
                        colNum -= cols;
                        new_translation.x = -5 - 10 * (rows - 1) / 2 + rowNum * 10;
                        new_translation.y = -5 - 10 * (cols - 1) / 2 + colNum * 10;
                        Vector3 axis(0, 0, 1);
                        Degree  d(90.0);
                        Radian  angle(d);
                        new_rotation.fromAngleAxis(angle, axis);
                    }
                    transform_component->setPosition(new_translation);
                    transform_component->setRotation(new_rotation);
                }
            }
        }

    }

    void Level::tick(float delta_time)
    {
        if (!m_is_loaded)
        {
            return;
        }

        for (const auto& id_object_pair : m_gobjects)
        {
            assert(id_object_pair.second);
            if (id_object_pair.second)
            {
                id_object_pair.second->tick(delta_time);
            }
        }
        if (m_current_active_character && g_is_editor_mode == false)
        {
            m_current_active_character->tick(delta_time);
        }

        std::shared_ptr<PhysicsScene> physics_scene = m_physics_scene.lock();
        if (physics_scene)
        {
            physics_scene->tick(delta_time);
        }
    }

    std::weak_ptr<GObject> Level::getGObjectByID(GObjectID go_id) const
    {
        auto iter = m_gobjects.find(go_id);
        if (iter != m_gobjects.end())
        {
            return iter->second;
        }

        return std::weak_ptr<GObject>();
    }

    void Level::deleteGObjectByID(GObjectID go_id)
    {
        auto iter = m_gobjects.find(go_id);
        if (iter != m_gobjects.end())
        {
            std::shared_ptr<GObject> object = iter->second;
            if (object)
            {
                if (m_current_active_character && m_current_active_character->getObjectID() == object->getID())
                {
                    m_current_active_character->setObject(nullptr);
                }
            }
        }

        m_gobjects.erase(go_id);
    }

} // namespace Piccolo
