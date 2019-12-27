#ifndef IMAGE_5_CODE_ADDRESSES
#define IMAGE_5_CODE_ADDRESSES

#define image_5_start_task_initialization_task		(0x18)
#define image_5_initialization_task		(0x18)
#define image_5_start_task_processing_wakeup_request		(0x4C)
#define image_5_processing_wakeup_request		(0x4C)
#define image_5_start_task_debug_routine		(0x2218)
#define image_5_debug_routine		(0x2218)
#define image_5_debug_routine_handler		(0xC)
#define image_5_processing_check_wlan_llcsnap		(0x44C)
#define image_5_processing_done		(0x64C)
#define image_5_action_gpe		(0x1394)
#define image_5_action_exception		(0x13A8)
#define image_5_gpe_sop_push_replace_ddr_sram_32		(0x13E8)
#define image_5_gpe_sop_push_replace_sram_32_64		(0x142C)
#define image_5_gpe_sop_push_replace_sram_64		(0x1440)
#define image_5_gpe_sop_push_replace_sram_64_32		(0x1454)
#define image_5_gpe_sop_pull_replace_ddr_sram_32		(0x1468)
#define image_5_gpe_sop_pull_replace_sram_32_64		(0x14AC)
#define image_5_gpe_sop_pull_replace_sram_64		(0x14FC)
#define image_5_gpe_sop_pull_replace_sram_64_32		(0x1538)
#define image_5_gpe_replace_pointer_32_sram		(0x1588)
#define image_5_gpe_replace_pointer_64_sram		(0x15AC)
#define image_5_gpe_replace_16		(0x15D0)
#define image_5_gpe_replace_32		(0x1604)
#define image_5_gpe_replace_bits_16		(0x1628)
#define image_5_gpe_copy_add_16_cl		(0x1654)
#define image_5_gpe_copy_add_16_sram		(0x1660)
#define image_5_gpe_copy_bits_16_cl		(0x16A8)
#define image_5_gpe_copy_bits_16_sram		(0x16B4)
#define image_5_gpe_insert_16		(0x16FC)
#define image_5_gpe_delete_16		(0x1764)
#define image_5_gpe_decrement_8		(0x17A4)
#define image_5_gpe_apply_icsum_16		(0x17C8)
#define image_5_gpe_apply_icsum_nz_16		(0x17EC)
#define image_5_gpe_compute_csum_16_cl		(0x1828)
#define image_5_gpe_compute_csum_16_sram		(0x1834)
#define image_5_gpe_buffer_copy_16_sram		(0x1874)
#define image_5_gpe_buffer_copy_16_ddr		(0x18A0)
#define image_5_tcam_cmd_parser_1		(0x195C)
#define image_5_tcam_cmd_parser_2		(0x1968)
#define image_5_tcam_cmd_parser_4		(0x1974)
#define image_5_tcam_cmd_parser_6		(0x1980)
#define image_5_tcam_cmd_parser_8		(0x19A0)
#define image_5_tcam_cmd_parser_8_not_aligned		(0x19AC)
#define image_5_tcam_cmd_packet		(0x19D8)
#define image_5_tcam_cmd_l3_hdr		(0x19EC)
#define image_5_tcam_cmd_l4_hdr		(0x19FC)
#define image_5_tcam_cmd_ingress_port		(0x1A54)
#define image_5_tcam_cmd_network_layer		(0x1A60)
#define image_5_tcam_cmd_gem_flow		(0x1A74)
#define image_5_tcam_cmd_src_ip		(0x1A80)
#define image_5_tcam_cmd_dst_ip		(0x1AA8)

#endif