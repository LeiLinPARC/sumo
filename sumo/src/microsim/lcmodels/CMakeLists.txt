set(microsim_lcmodels_STAT_SRCS
   MSAbstractLaneChangeModel.cpp
   MSAbstractLaneChangeModel.h
   MSLCM_DK2008.cpp
   MSLCM_DK2008.h
   MSLCM_LC2013.cpp
   MSLCM_LC2013.h
   MSLCM_SL2015.cpp
   MSLCM_SL2015.h
)

add_library(microsim_lcmodels STATIC ${microsim_lcmodels_STAT_SRCS})
