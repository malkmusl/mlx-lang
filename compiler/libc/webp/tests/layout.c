/* Prints the sizes and field offsets of libwebp's public structures and
   the values of its enumerations, to compare the system's headers with
   mlx-libwebp's. */
#include <stdio.h>
#include <stddef.h>
#include <webp/decode.h>
#include <webp/demux.h>
#include <webp/mux_types.h>
#ifdef WITH_ENCODER
#include <webp/encode.h>
#include <webp/mux.h>
#endif
#define S(t) printf("sizeof " #t " %zu\n", sizeof(t))
#define F(t, f) printf(#t "." #f " %zu\n", offsetof(t, f))
#define V(e) printf(#e " %d\n", (int)(e))
int main(void) {
  S(WebPRGBABuffer); S(WebPYUVABuffer); S(WebPDecBuffer); S(WebPBitstreamFeatures); S(WebPDecoderOptions); S(WebPDecoderConfig);
  S(WebPData); S(WebPIterator); S(WebPChunkIterator); S(WebPAnimDecoderOptions); S(WebPAnimInfo);
  F(WebPRGBABuffer, stride); F(WebPRGBABuffer, size);
  F(WebPYUVABuffer, a); F(WebPYUVABuffer, y_stride); F(WebPYUVABuffer, a_stride); F(WebPYUVABuffer, y_size); F(WebPYUVABuffer, a_size);
  F(WebPDecBuffer, is_external_memory); F(WebPDecBuffer, u); F(WebPDecBuffer, pad); F(WebPDecBuffer, private_memory);
  F(WebPBitstreamFeatures, format); F(WebPBitstreamFeatures, pad);
  F(WebPDecoderOptions, crop_height); F(WebPDecoderOptions, use_threads); F(WebPDecoderOptions, alpha_dithering_strength); F(WebPDecoderOptions, pad);
  F(WebPDecoderConfig, output); F(WebPDecoderConfig, options);
  F(WebPIterator, duration); F(WebPIterator, dispose_method); F(WebPIterator, complete); F(WebPIterator, fragment); F(WebPIterator, has_alpha);
  F(WebPIterator, blend_method); F(WebPIterator, pad); F(WebPIterator, private_);
  F(WebPChunkIterator, chunk); F(WebPChunkIterator, pad); F(WebPChunkIterator, private_);
  F(WebPAnimDecoderOptions, use_threads); F(WebPAnimDecoderOptions, padding);
  F(WebPAnimInfo, frame_count); F(WebPAnimInfo, pad);
  V(WEBP_DECODER_ABI_VERSION); V(WEBP_DEMUX_ABI_VERSION);
  V(MODE_RGB); V(MODE_rgbA); V(MODE_rgbA_4444); V(MODE_YUVA); V(MODE_LAST);
  V(VP8_STATUS_OK); V(VP8_STATUS_SUSPENDED); V(VP8_STATUS_NOT_ENOUGH_DATA);
  V(WEBP_DEMUX_PARSE_ERROR); V(WEBP_DEMUX_DONE); V(WEBP_FF_FRAME_COUNT); V(WEBP_FF_BACKGROUND_COLOR);
  V(ANIMATION_FLAG); V(ALPHA_FLAG); V(ICCP_FLAG); V(ALL_VALID_FLAGS);
  V(WEBP_MUX_DISPOSE_BACKGROUND); V(WEBP_MUX_NO_BLEND);
  V(WebPIsPremultipliedMode(MODE_Argb)); V(WebPIsAlphaMode(MODE_YUVA)); V(WebPIsAlphaMode(MODE_RGB)); V(WebPIsRGBMode(MODE_YUV));
#ifdef WITH_ENCODER
  S(WebPConfig); S(WebPPicture); S(WebPAuxStats); S(WebPMemoryWriter);
  F(WebPConfig, image_hint); F(WebPConfig, alpha_quality); F(WebPConfig, use_sharp_yuv); F(WebPConfig, qmax);
  F(WebPPicture, y); F(WebPPicture, a); F(WebPPicture, argb); F(WebPPicture, argb_stride); F(WebPPicture, writer); F(WebPPicture, custom_ptr);
  F(WebPPicture, stats); F(WebPPicture, error_code); F(WebPPicture, progress_hook); F(WebPPicture, user_data); F(WebPPicture, pad4);
  F(WebPPicture, memory_); F(WebPPicture, memory_argb_); F(WebPPicture, pad7);
  F(WebPAuxStats, residual_bytes); F(WebPAuxStats, lossless_features); F(WebPAuxStats, lossless_data_size); F(WebPAuxStats, pad);
  F(WebPMemoryWriter, max_size); F(WebPMemoryWriter, pad);
  V(WEBP_ENCODER_ABI_VERSION); V(WEBP_MUX_ABI_VERSION); V(WEBP_MAX_DIMENSION); V(WEBP_HINT_GRAPH); V(WEBP_PRESET_TEXT);
  V(WEBP_YUV420A); V(VP8_ENC_ERROR_USER_ABORT); V(VP8_ENC_ERROR_LAST);
  V(WEBP_MUX_OK); V(WEBP_MUX_NOT_FOUND); V(WEBP_MUX_NOT_ENOUGH_DATA); V(WEBP_CHUNK_ANMF); V(WEBP_CHUNK_NIL);
  S(WebPMuxFrameInfo); S(WebPMuxAnimParams); S(WebPAnimEncoderOptions);
  F(WebPMuxFrameInfo, x_offset); F(WebPMuxFrameInfo, id); F(WebPMuxFrameInfo, dispose_method); F(WebPMuxFrameInfo, pad);
  F(WebPAnimEncoderOptions, anim_params); F(WebPAnimEncoderOptions, minimize_size); F(WebPAnimEncoderOptions, verbose); F(WebPAnimEncoderOptions, padding);
#endif
  return 0;
}
