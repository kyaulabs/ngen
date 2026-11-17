include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

if(BUILD_SHARED_LIBS)
  message(FATAL_ERROR "NGEN_INSTALL_SDK supports only source-built static NGEN libraries")
endif()

set(ngen_sdk_cmake_dir "${CMAKE_INSTALL_LIBDIR}/cmake/NGEN")
set(ngen_sdk_include_dir "${CMAKE_INSTALL_INCLUDEDIR}/ngen")
set(ngen_sdk_doc_dir "${CMAKE_INSTALL_DATADIR}/doc/ngen")

function(ngen_sdk_headers target)
  target_sources(${target} PUBLIC FILE_SET HEADERS BASE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/engine"
    FILES ${ARGN})
  set_target_properties(${target} PROPERTIES EXPORT_NO_SYSTEM ON)
endfunction()

ngen_sdk_headers(ngen_core engine/core/entities.hpp engine/core/values.hpp)
ngen_sdk_headers(ngen_jobs engine/jobs/jobs.hpp)
ngen_sdk_headers(ngen_simulation engine/simulation/simulation.hpp)
set_target_properties(ngen_options PROPERTIES EXPORT_NAME detail_options)
set_target_properties(ngen_core PROPERTIES EXPORT_NAME core)
set_target_properties(ngen_jobs PROPERTIES EXPORT_NAME jobs)
set_target_properties(ngen_simulation PROPERTIES EXPORT_NAME simulation)
install(TARGETS ngen_options ngen_core ngen_jobs ngen_simulation EXPORT NGENCoreTargets
  ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
  FILE_SET HEADERS DESTINATION "${ngen_sdk_include_dir}")
install(EXPORT NGENCoreTargets NAMESPACE NGEN:: DESTINATION "${ngen_sdk_cmake_dir}")

if(TARGET ngen_assets)
  ngen_sdk_headers(ngen_assets engine/assets/package.hpp)
  set_target_properties(ngen_assets PROPERTIES EXPORT_NAME assets)
  install(TARGETS ngen_assets EXPORT NGENAssetsTargets
    ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    FILE_SET HEADERS DESTINATION "${ngen_sdk_include_dir}")
  install(EXPORT NGENAssetsTargets NAMESPACE NGEN:: DESTINATION "${ngen_sdk_cmake_dir}")
endif()

if(TARGET ngen_render)
  ngen_sdk_headers(ngen_render_contract engine/render/capabilities.hpp)
  ngen_sdk_headers(ngen_render engine/render/context.hpp engine/render/environment.hpp
    engine/render/mesh.hpp engine/render/resources.hpp engine/render/shader_contract.hpp
    engine/render/surface.hpp)
  set_target_properties(ngen_render_contract PROPERTIES EXPORT_NAME render_contract)
  set_target_properties(ngen_render PROPERTIES EXPORT_NAME renderer)
  set_target_properties(ngen_vma PROPERTIES EXPORT_NAME detail_vma)
  install(TARGETS ngen_render_contract ngen_render ngen_vma EXPORT NGENRendererTargets
    ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    FILE_SET HEADERS DESTINATION "${ngen_sdk_include_dir}")
  install(EXPORT NGENRendererTargets NAMESPACE NGEN:: DESTINATION "${ngen_sdk_cmake_dir}")
  install(FILES docs/evidence/M2/gpu-resources/licenses/vma.txt
    DESTINATION "${ngen_sdk_doc_dir}/licenses")
endif()

configure_package_config_file(cmake/NGENConfig.cmake.in "${CMAKE_CURRENT_BINARY_DIR}/NGENConfig.cmake"
  INSTALL_DESTINATION "${ngen_sdk_cmake_dir}")
write_basic_package_version_file("${CMAKE_CURRENT_BINARY_DIR}/NGENConfigVersion.cmake"
  COMPATIBILITY ExactVersion)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/NGENConfig.cmake"
  "${CMAKE_CURRENT_BINARY_DIR}/NGENConfigVersion.cmake" DESTINATION "${ngen_sdk_cmake_dir}")
install(FILES LICENSE DESTINATION "${ngen_sdk_doc_dir}")
