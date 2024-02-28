; $MODE = "UniformRegister"
; $ATTRIB_VARS[0].name = "position"
; $ATTRIB_VARS[0].type = "float2"
; $ATTRIB_VARS[0].location = 0
; $ATTRIB_VARS[1].name = "tex_coords"
; $ATTRIB_VARS[1].type = "float2"
; $ATTRIB_VARS[1].location = 1
; $ATTRIB_VARS[2].name = "vtx_color"
; $ATTRIB_VARS[2].type = "float4"
; $ATTRIB_VARS[2].location = 2
; $SPI_VS_OUT_CONFIG.VS_EXPORT_COUNT = 1
; $NUM_SPI_VS_OUT_ID = 1
; $SPI_VS_OUT_ID[0].SEMANTIC_0 = 0
; $SPI_VS_OUT_ID[0].SEMANTIC_1 = 1

00 CALL_FS NO_BARRIER
01 EXP_DONE: POS0, R1
02 EXP_DONE: PARAM0, R2 NO_BARRIER
03 EXP_DONE: PARAM1, R3 NO_BARRIER
END_OF_PROGRAM
