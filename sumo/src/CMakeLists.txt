include_directories(${CMAKE_SOURCE_DIR}/src)
include_directories(${CMAKE_BINARY_DIR}/src)

set(commonlibs
        utils_distribution utils_shapes utils_options
        utils_xml utils_geom utils_common utils_importio utils_iodevices foreign_tcpip xerces-c proj)

set(commonvehiclelibs
        utils_emissions foreign_phemlight utils_vehicle ${commonlibs} ${FOX_LIBRARIES})

set(netconvertlibs
        netwrite netimport netbuild foreign_eulerspiral gdal netimport_vissim netimport_vissim_typeloader netimport_vissim_tempstructs ${commonlibs})

add_subdirectory(activitygen)
add_subdirectory(dfrouter)
add_subdirectory(duarouter)
add_subdirectory(foreign)
add_subdirectory(gui)
add_subdirectory(guinetload)
add_subdirectory(guisim)
add_subdirectory(jtrrouter)
add_subdirectory(marouter)
add_subdirectory(mesogui)
add_subdirectory(mesosim)
add_subdirectory(microsim)
add_subdirectory(netbuild)
add_subdirectory(netedit)
add_subdirectory(netgen)
add_subdirectory(netimport)
add_subdirectory(netload)
add_subdirectory(netwrite)
add_subdirectory(od)
add_subdirectory(osgview)
add_subdirectory(polyconvert)
add_subdirectory(router)
add_subdirectory(tools)
add_subdirectory(traci-server)
add_subdirectory(traci_testclient)
add_subdirectory(utils)


if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
    set(sumoexec
            sumo_main.cpp
            microsim/MSMoveReminder.cpp microsim/MSMoveReminder.h)
    set(sumoguiexec
            guisim_main.cpp
            microsim/MSMoveReminder.cpp microsim/MSMoveReminder.h
            gui/GUIManipulator.h gui/GUIManipulator.cpp
            gui/GUITLLogicPhasesTrackerWindow.h gui/GUITLLogicPhasesTrackerWindow.cpp
            mesosim/MEVehicleControl.h mesosim/MEVehicleControl.cpp)
else ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
    set(sumoexec
            sumo_main.cpp)
    set(sumoguiexec
            guisim_main.cpp)
endif ()

set(sumolibs
        netload microsim microsim_cfmodels microsim_lcmodels microsim_devices microsim_output microsim_pedestrians microsim_trigger microsim_actions
        microsim_traffic_lights mesosim traciserver traciserver_lib ${commonvehiclelibs} proj ${GRPC_LIBS}
        ${PYTHON_LIBRARIES})

add_executable(sumo version.h ${sumoexec})
target_link_libraries(sumo ${sumolibs})

add_executable(sumo-gui version.h ${sumoguiexec})
target_link_libraries(sumo-gui gui guinetload guisim gui_dialogs utils_gui_windows utils_gui_globjects utils_gui_div utils_gui_settings
        utils_gui_images utils_gui_tracker utils_foxtools osgview ${sumolibs} mesogui gdal ${FFMPEG_LIBRARIES} foreign_gl2ps foreign_polyfonts)

add_executable(netconvert version.h netconvert_main.cpp)
target_link_libraries(netconvert ${netconvertlibs})

add_executable(od2trips version.h od2trips_main.cpp)
target_link_libraries(od2trips od utils_vehicle ${commonlibs})

configure_file(config.h.cmake config.h)

add_custom_command(OUTPUT version.h
                   COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/../tools/build/version.py ${CMAKE_CURRENT_SOURCE_DIR}
#                   DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/../.svn/wc.db
                   )
