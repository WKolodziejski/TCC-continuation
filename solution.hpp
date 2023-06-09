#ifndef WILLIAM_H
#define WILLIAM_H

#include <opencv2/xfeatures2d.hpp>
#include <opencv2/features2d.hpp>
#include "av1/corner_match.h"

#ifdef __cplusplus
extern "C" {
#endif

using namespace std;

#define WARP_BLOCK_SIZE 32

struct MatrixMap {
  double mat[8]{};
  int64_t error = INT64_MAX;
  int k = 0;
  bool zero_motion = false;
};

struct Stats {
  int64_t seg_error = INT64_MAX;
  int64_t error = INT64_MAX;
  int64_t k_error = INT64_MAX;
  int64_t seg_gain = INT64_MAX;
  int64_t k_gain = INT64_MAX;
  double seg_percent = HUGE_VAL;
  double k_percent = HUGE_VAL;
};

enum Detect {
  FAST_BEBLID,
  FAST_SIFT,
  FAST_SURF,
  FAST_BRISK,
  FAST_BRIEF,
  FAST_FREAK,
  FAST_DAISY,
  SIFT,
  SURF,
  ORB,
  BRISK,
  KAZE,
  AKAZE,
  FAST_AOM
};

enum Match { FLANN_BEST, FLANN_KNN, BF_BEST, BF_KNN, BF_AOM };

enum Estimate {
  RANSAC,
  MLESAC,
  MSAC,
  PROSAC,
  PROMSAC,
  PROMLESAC,
  LMEDS,
  RANSAC_AOM
};

void compute(const cv::Mat &src_frame, const cv::Mat &ref_frame,
             TransformationType transformation_type, Detect detect_type,
             Match match_type, Estimate estimate_type, Stats &stats, int frame);

void av1(const cv::Mat &src_frame, const cv::Mat &ref_frame,
         TransformationType transformation_type, Stats &stats, int frame);

string formatNameSolution(Detect detect_type, Match match_type, Estimate estimate_type,
                  int frame);

string detectName(int d);

string matchName(int m);

string describeName(int d);

string modeName(int m);

string estimateName(int e);

#ifdef __cplusplus
}
#endif

#endif  // WILLIAM_H
