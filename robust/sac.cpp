#ifndef __SAC__
#define __SAC__

#include <stdio.h>
#include "sac.hpp"
#include "../av1/ransac.c"
#include "../opencv/draw.hpp"
#include "../utils.hpp"
#include <cmath>

RTL::Estimator<RTL::Affine, Correspondence, Correspondence *> *getEstimator(
    TransformationType type) {
  switch (type) {
    case AFFINE: return new AffineEstimator();
    case ROTZOOM: return new RotZoomEstimator();
    case TRANSLATION: return new TranslationEstimator();
    default: assert(0);
  }
}

void estimate_clustered(Mat &src_img, Mat &ref_img,
                        Correspondence *correspondences,
                        int num_correspondences,
                        TransformationType transformation_type, Estimate type,
                        Stats &stats, int frame) {
  draw_clustered_motion_field(src_img, ref_img, correspondences,
                              num_correspondences,
                              formatName("clustered_motion_field", frame));

  Mat labels;
  Mat centers;
  Mat samples(num_correspondences, 1, CV_32F);

  // Calcula distâncias dos MVs nas correspondências
  for (int i = 0; i < num_correspondences; i++) {
    samples.at<float>(i) = distance(correspondences[i]);
  }

  double last_error = 0;
  int k = 1;
  int a = 0;

  // Escolhe o melhor valor de K usando a regra do joelho
  for (int i = k; i < 10; i++) {
    if (num_correspondences < i) {
      break;
    }

    double error = kmeans(samples, i, labels, TermCriteria(3, 10000, 0.0001), 5,
                          KMEANS_PP_CENTERS, centers);

    if (error < last_error / 2) {
      k = i;
      a++;
    }

    // if attempts > 2 stop increasing k
    if (a > 1) {
      break;
    }

    last_error = error;
  }

  int x = floor(src_img.size().width / WARP_BLOCK_SIZE);
  int y = floor(src_img.size().height / WARP_BLOCK_SIZE);

  auto **map = new MatrixMap *[x];
  for (int xi = 0; xi < x; xi++) {
    map[xi] = new MatrixMap[y];
  }

  labels.release();
  centers.release();

  kmeans(samples, k, labels, TermCriteria(3, 10000, 0.0001), 5,
         KMEANS_PP_CENTERS, centers);

  vector<Correspondence> clusters[k];

  // Popula os clusters com as respectivas correspondências
  for (int i = 0; i < num_correspondences; i++) {
    Correspondence c = correspondences[i];
    clusters[labels.at<int>(i)].push_back(c);
  }

  // Escolhe a melhor matriz afim, dentre as K geradas
  for (int i = 0; i < k; i++) {
    size_t size = clusters[i].size();

    if (size < 3) {
      continue;
    }

    double mat[8];

    // Estima quantidade de inliers para o cluster atual
    double inliers = estimate(clusters[i].data(), size, transformation_type,
                              type, stats, mat);

    if (inliers < 0.01) {
      continue;
    }

    double err = calc_error(src_img, ref_img, mat);

    if (err < stats.k_error) {
      stats.k_error = err;
    }

    calc_seg_error(src_img, ref_img, mat, x, y, map, i,
                   distance(clusters[i].data()[1]) == 0);
  }

  stats.seg_error = 0;

  for (int xi = 0; xi < x; xi++) {
     for (int yj = 0; yj < y; yj++) {
      stats.seg_error += map[xi][yj].error;
     }
  }

  draw_k_warped_image(src_img, ref_img, x, y, map,
                      formatName("inv_clustered_warped", frame),
                      formatName("fwd_clustered_warped", frame));
}

double estimate(Correspondence *correspondences, int num_correspondences,
                TransformationType transformation_type, Estimate type,
                Stats &stats, double mat[8]) {
  RTL::Estimator<RTL::Affine, Correspondence, Correspondence *> *estimator =
      getEstimator(transformation_type);
  MotionModel params_by_motion[1];
  params_by_motion[0].num_inliers = 0;
  params_by_motion[0].inliers = static_cast<int *>(
      malloc(sizeof(*(params_by_motion[0].inliers)) * 2 * num_correspondences));

  if (type == Estimate::RANSAC) {
    RTL::RANSAC<RTL::Affine, Correspondence, Correspondence *> ransac(
        estimator);
    ransac.compute(correspondences, num_correspondences, params_by_motion);

  } else if (type == Estimate::MLESAC) {
    RTL::MLESAC<RTL::Affine, Correspondence, Correspondence *> ransac(
        estimator);
    ransac.compute(correspondences, num_correspondences, params_by_motion);

  } else if (type == Estimate::MSAC) {
    RTL::MSAC<RTL::Affine, Correspondence, Correspondence *> ransac(estimator);
    ransac.compute(correspondences, num_correspondences, params_by_motion);

  } else if (type == Estimate::PROSAC) {
    RTL::PROSAC<RTL::Affine, Correspondence, Correspondence *> ransac(
        estimator);
    ransac.compute(correspondences, num_correspondences, params_by_motion);

  } else if (type == Estimate::LMEDS) {
    RTL::LMedS<RTL::Affine, Correspondence, Correspondence *> ransac(estimator);
    ransac.compute(correspondences, num_correspondences, params_by_motion);

  } else if (type == Estimate::PROMSAC) {
    RTL::PROMSAC<RTL::Affine, Correspondence, Correspondence *> ransac(
        estimator);
    ransac.compute(correspondences, num_correspondences, params_by_motion);

  } else if (type == Estimate::PROMLESAC) {
    RTL::PROMLESAC<RTL::Affine, Correspondence, Correspondence *> ransac(
        estimator);
    ransac.compute(correspondences, num_correspondences, params_by_motion);

  } else if (type == Estimate::RANSAC_AOM) {
    int num_inliers_by_motion[1];
    RansacFunc ransac = av1_get_ransac_type(transformation_type);
    ransac((int *)correspondences, num_correspondences, num_inliers_by_motion,
           params_by_motion, 1);

  } else {
    assert(0);
  }

  int outliers_num = num_correspondences - params_by_motion->num_inliers;
  int inliers_num = params_by_motion->num_inliers;
  double inliers_per = (float)inliers_num / (float)num_correspondences * 100;

  for (int i = 0; i < 8; i++) {
    mat[i] = params_by_motion->params[i];
  }

  free(params_by_motion[0].inliers);

  delete estimator;

  return inliers_per;
}

// void estimate_av1(int *correspondences, int num_correspondences,
//                   TransformationType transformation_type, Stats &stats) {
//   int num_inliers_by_motion[1];
//   MotionModel params_by_motion[1];
//   params_by_motion[0].num_inliers = 0;
//   params_by_motion[0].inliers = static_cast<int *>(
//       malloc(sizeof(*(params_by_motion[0].inliers)) * 2 *
//       num_correspondences));
//
//   RansacFunc ransac = av1_get_ransac_type(transformation_type);
//   ransac(correspondences, num_correspondences, num_inliers_by_motion,
//          params_by_motion, 1);
//
//   stats.outliers_num = num_correspondences - num_inliers_by_motion[0];
//   stats.inliers_num = num_inliers_by_motion[0];
//   stats.inliers_per =
//       stats.matches_num == 0
//           ? 0.0
//           : (float)stats.inliers_num / (float)stats.matches_num * 100;
//
//   // Pontos: inliers / outliers
//   //  fprintf(stderr, "%d / %d : %f\n", params_by_motion->num_inliers,
//   outliers,
//   //  p);
//
//   free(params_by_motion[0].inliers);
// }

#endif  // End of '__SAC__'