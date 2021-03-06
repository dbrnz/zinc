# OpenCMISS-Zinc Library Unit Tests
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

SET(CURRENT_TEST fieldio)
LIST(APPEND API_TESTS ${CURRENT_TEST})
SET(${CURRENT_TEST}_SRC
	${CURRENT_TEST}/fieldml_basic.cpp
	${CURRENT_TEST}/fieldml_hermite.cpp
	utilities/fileio.cpp
	)

SET(FIELDIO_FIELDML_CUBE_RESOURCE "${CMAKE_CURRENT_LIST_DIR}/cube.fieldml")
SET(FIELDIO_FIELDML_TETMESH_RESOURCE "${CMAKE_CURRENT_LIST_DIR}/tetmesh.fieldml")
SET(FIELDIO_FIELDML_WHEEL_DIRECT_RESOURCE "${CMAKE_CURRENT_LIST_DIR}/wheel_direct.fieldml")
SET(FIELDIO_FIELDML_WHEEL_INDIRECT_RESOURCE "${CMAKE_CURRENT_LIST_DIR}/wheel_indirect.fieldml")
SET(FIELDIO_EX_LINES_UNIT_SCALE_FACTORS_RESOURCE "${CMAKE_CURRENT_LIST_DIR}/lines_unit_scale_factors.exfile")
SET(FIELDIO_EX_LINES_ALTERNATE_NODE_ORDER_RESOURCE "${CMAKE_CURRENT_LIST_DIR}/lines_alternate_node_order.exfile")
SET(FIELDIO_EX_LINES_INCONSISTENT_NODE_ORDER_RESOURCE "${CMAKE_CURRENT_LIST_DIR}/lines_inconsistent_node_order.exfile")
SET(FIELDIO_EX_HERMITE_FIGURE8_RESOURCE "${CMAKE_CURRENT_LIST_DIR}/figure8.exfile")
SET(FIELDIO_EX_TWOHERMITECUBES_NOSCALEFACTORS_RESOURCE "${CMAKE_CURRENT_LIST_DIR}/twohermitecubes_noscalefactors.exfile")
