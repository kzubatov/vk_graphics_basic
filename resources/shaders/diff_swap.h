#ifndef DIFF_SWAP_H
#define DIFF_SWAP_H

#ifndef __cplusplus
  #define x_swap(left_texel_src, right_texel_src, dst, bool_vec) \
    dst =  bool_vec ? right_texel_src : left_texel_src; \
    dst += dFdxFine(dst) * t.x;

  #define y_swap(top_texel_src, bottom_texel_src, dst, bool_vec) \
    dst =  bool_vec ? bottom_texel_src : top_texel_src; \
    dst += dFdyFine(dst) * t.y;
#endif

#endif