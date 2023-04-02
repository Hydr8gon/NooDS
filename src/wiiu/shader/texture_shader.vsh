; $MODE = "UniformRegister"
; $ATTRIB_VARS[0].name = "position"
; $ATTRIB_VARS[0].type = "Float2"
; $ATTRIB_VARS[0].location = 0
; $ATTRIB_VARS[1].name = "tex_coord_in"
; $ATTRIB_VARS[1].type = "Float2"
; $ATTRIB_VARS[1].location = 1
; $NUM_SPI_VS_OUT_ID = 1
; $SPI_VS_OUT_ID[0].SEMANTIC_0 = 0

00 CALL_FS NO_BARRIER
01 ALU: ADDR(32) CNT(4)
       0  x: MOV         R2.x,  R2.x
          y: MOV         R2.y,  R2.y
          z: MOV         R1.z,  0.0f
          w: MOV         R1.w,  (0x3F800000, 1.0f).x
02 EXP_DONE: POS0, R1
03 EXP_DONE: PARAM0, R2.xyzz  NO_BARRIER
END_OF_PROGRAM