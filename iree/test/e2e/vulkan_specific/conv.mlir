func @conv() attributes { iree.module.export } {
  %0 = iree.unfoldable_constant dense<
      [[[[0.5       , 0.5212766 ],
         [0.54255319, 0.56382979],
         [0.58510638, 0.60638298],
         [0.62765957, 0.64893617],
         [0.67021277, 0.69148936],
         [0.71276596, 0.73404255]],

        [[0.75531915, 0.77659574],
         [0.79787234, 0.81914894],
         [0.84042553, 0.86170213],
         [0.88297872, 0.90425532],
         [0.92553191, 0.94680851],
         [0.96808511, 0.9893617 ]],

        [[1.0106383 , 1.03191489],
         [1.05319149, 1.07446809],
         [1.09574468, 1.11702128],
         [1.13829787, 1.15957447],
         [1.18085106, 1.20212766],
         [1.22340426, 1.24468085]],

        [[1.26595745, 1.28723404],
         [1.30851064, 1.32978723],
         [1.35106383, 1.37234043],
         [1.39361702, 1.41489362],
         [1.43617021, 1.45744681],
         [1.4787234 , 1.5       ]]]]> : tensor<1x4x6x2xf32>
  %1 = iree.unfoldable_constant dense<
      [[[[0.5       , 0.52857143, 0.55714286],
         [0.58571429, 0.61428571, 0.64285714]],

        [[0.67142857, 0.7       , 0.72857143],
         [0.75714286, 0.78571429, 0.81428571]],

        [[0.84285714, 0.87142857, 0.9       ],
         [0.92857143, 0.95714286, 0.98571429]]],


       [[[1.01428571, 1.04285714, 1.07142857],
         [1.1       , 1.12857143, 1.15714286]],

        [[1.18571429, 1.21428571, 1.24285714],
         [1.27142857, 1.3       , 1.32857143]],

        [[1.35714286, 1.38571429, 1.41428571],
         [1.44285714, 1.47142857, 1.5       ]]]]>
   : tensor<2x3x2x3xf32>
  %2 = "mhlo.convolution"(%0, %1) {
       batch_group_count = 1 : i64,
       dimension_numbers = {
         input_batch_dimension = 0 : i64,
   input_feature_dimension = 3 : i64,
   input_spatial_dimensions = dense<[1, 2]> : tensor<2xi64>,
   kernel_input_feature_dimension = 2 : i64,
   kernel_output_feature_dimension = 3 : i64,
   kernel_spatial_dimensions = dense<[0, 1]> : tensor<2xi64>,
   output_batch_dimension = 0 : i64,
   output_feature_dimension = 3 : i64,
   output_spatial_dimensions = dense<[1, 2]> : tensor<2xi64>},
       feature_group_count = 1 : i64,
       rhs_dilation = dense<1> : tensor<2xi64>,
       window_strides = dense<1> : tensor<2xi64>}
       : (tensor<1x4x6x2xf32>, tensor<2x3x2x3xf32>) -> (tensor<1x3x4x3xf32>)
   check.expect_almost_eq_const(%2, dense<
         [[[[ 8.39452888,  8.62796353,  8.86139818],
         [ 8.89057751,  9.13860182,  9.38662614],
         [ 9.38662614,  9.64924012,  9.9118541 ],
         [ 9.88267477, 10.15987842, 10.43708207]],

        [[11.37082067, 11.69179331, 12.01276596],
         [11.8668693 , 12.20243161, 12.53799392],
         [12.36291793, 12.71306991, 13.06322188],
         [12.85896657, 13.22370821, 13.58844985]],

        [[14.34711246, 14.7556231 , 15.16413374],
         [14.84316109, 15.2662614 , 15.6893617 ],
         [15.33920973, 15.7768997 , 16.21458967],
         [15.83525836, 16.28753799, 16.73981763]]]]>
        : tensor<1x3x4x3xf32>) : tensor<1x3x4x3xf32>
   return
}

func @conv_1d() {
  %inputs = iree.unfoldable_constant dense<2.0> : tensor<3x8x1xf32>
  %weights = iree.unfoldable_constant dense<2.0> : tensor<3x1x1xf32>
  %res = "mhlo.convolution"(%inputs, %weights) {
    batch_group_count = 1 : i64,
    dimension_numbers = {
      input_batch_dimension = 0 : i64,
      input_feature_dimension = 2 : i64,
      input_spatial_dimensions = dense<[1]> : tensor<1xi64>,
      kernel_input_feature_dimension = 1 : i64,
      kernel_output_feature_dimension = 2 : i64,
      kernel_spatial_dimensions = dense<[0]> : tensor<1xi64>,
      output_batch_dimension = 0 : i64,
      output_feature_dimension = 2 : i64,
      output_spatial_dimensions = dense<[1]> : tensor<1xi64>
    },
    feature_group_count = 1 : i64,
    padding = dense<0> : tensor<1x2xi64>,
    rhs_dilation = dense<1> : tensor<1xi64>,
    window_strides = dense<1> : tensor<1xi64>
  } : (tensor<3x8x1xf32>, tensor<3x1x1xf32>) -> tensor<3x6x1xf32>
  check.expect_almost_eq_const(%res, dense<12.0> : tensor<3x6x1xf32>) : tensor<3x6x1xf32>
  return
}

func @conv_3d() {
  %inputs = iree.unfoldable_constant dense<1.0> : tensor<2x8x8x8x3xf32>
  %weights = iree.unfoldable_constant dense<1.0> : tensor<2x2x2x3x2xf32>
  %res = "mhlo.convolution"(%inputs, %weights) {
    batch_group_count = 1 : i64,
    dimension_numbers = {
      input_batch_dimension = 0 : i64,
      input_feature_dimension = 4 : i64,
      input_spatial_dimensions = dense<[1, 2, 3]> : tensor<3xi64>,
      kernel_input_feature_dimension = 3 : i64,
      kernel_output_feature_dimension = 4 : i64,
      kernel_spatial_dimensions = dense<[0, 1, 2]> : tensor<3xi64>,
      output_batch_dimension = 0 : i64,
      output_feature_dimension = 4 : i64,
      output_spatial_dimensions = dense<[1, 2, 3]> : tensor<3xi64>
    },
    feature_group_count = 1 : i64,
    padding = dense<0> : tensor<3x2xi64>,
    rhs_dilation = dense<1> : tensor<3xi64>,
    window_strides = dense<1> : tensor<3xi64>
  } : (tensor<2x8x8x8x3xf32>, tensor<2x2x2x3x2xf32>) -> tensor<2x7x7x7x2xf32>
  check.expect_almost_eq_const(%res, dense<24.0> : tensor<2x7x7x7x2xf32>) : tensor<2x7x7x7x2xf32>
  return
}
