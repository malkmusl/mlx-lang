/* Prints the sizes and field offsets of libjpeg's public structures, to
   compare the system's jpeglib.h with mlx-libjpeg's. */
#include <stdio.h>
#include <stddef.h>
#include <jpeglib.h>
#include <jerror.h>
#define F(s, f) printf(#s "." #f " %zu\n", offsetof(struct s, f))
int main(void) {
  printf("decompress %zu compress %zu error %zu component %zu source %zu dest %zu memory %zu marker %zu quant %zu huff %zu progress %zu last %d\n",
    sizeof(struct jpeg_decompress_struct), sizeof(struct jpeg_compress_struct), sizeof(struct jpeg_error_mgr), sizeof(jpeg_component_info),
    sizeof(struct jpeg_source_mgr), sizeof(struct jpeg_destination_mgr), sizeof(struct jpeg_memory_mgr), sizeof(struct jpeg_marker_struct),
    sizeof(JQUANT_TBL), sizeof(JHUFF_TBL), sizeof(struct jpeg_progress_mgr), (int)JMSG_LASTMSGCODE);
  F(jpeg_decompress_struct, src); F(jpeg_decompress_struct, output_gamma); F(jpeg_decompress_struct, colormap);
  F(jpeg_decompress_struct, coef_bits); F(jpeg_decompress_struct, comp_info); F(jpeg_decompress_struct, arith_dc_L);
  F(jpeg_decompress_struct, X_density); F(jpeg_decompress_struct, Adobe_transform); F(jpeg_decompress_struct, marker_list);
  F(jpeg_decompress_struct, sample_range_limit); F(jpeg_decompress_struct, MCU_membership); F(jpeg_decompress_struct, natural_order);
  F(jpeg_decompress_struct, unread_marker); F(jpeg_decompress_struct, master); F(jpeg_decompress_struct, cquantize);
  F(jpeg_compress_struct, input_gamma); F(jpeg_compress_struct, comp_info); F(jpeg_compress_struct, scan_info);
  F(jpeg_compress_struct, X_density); F(jpeg_compress_struct, next_scanline); F(jpeg_compress_struct, natural_order);
  F(jpeg_compress_struct, master); F(jpeg_compress_struct, script_space_size);
  F(jpeg_error_mgr, msg_code); F(jpeg_error_mgr, msg_parm); F(jpeg_error_mgr, trace_level); F(jpeg_error_mgr, num_warnings);
  F(jpeg_error_mgr, jpeg_message_table); F(jpeg_error_mgr, last_addon_message);
  printf("component.quant_table %zu\n", offsetof(jpeg_component_info, quant_table));
  return 0;
}
