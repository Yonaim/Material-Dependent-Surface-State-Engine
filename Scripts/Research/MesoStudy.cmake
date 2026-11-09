# Copy the production CPU implementation verbatim and expose only its private post-height stages.
# This enables a reference height to use the SAME normal/concavity fitting. No product API or solver changes.
set(MesoOriginal "${CMAKE_CURRENT_SOURCE_DIR}/Source/SurfaceState/Geometry/MesoGeometryBuilder.cpp")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${MesoOriginal}")
file(READ "${MesoOriginal}" MesoBody)
set(MesoGenerated "${CMAKE_BINARY_DIR}/research/MesoGeometryStudy.cpp")
file(WRITE "${MesoGenerated}" "${MesoBody}\nnamespace MDSS::SurfaceState {\nvoid RebuildMesoStudyGeometry(TSharedSurfaceGeometryData& Geometry) {\nauto& Texels=Geometry.GetTexels(); std::vector<std::uint8_t> Active(Texels.size(),0);\nfor(std::size_t I=0;I<Texels.size();++I) Active[I]=IsActive(Texels[I])?1:0;\nBuildDifferentialGeometry(Texels,Active); BuildCombinedConcavity(Texels);\n}\n}\n")
