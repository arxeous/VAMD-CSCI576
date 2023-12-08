#define _USE_MATH_DEFINES

#include <fftw3.h>
#include "math.h"
#include "AudioFile.h"
#include <chrono>
#include <iostream>
#include <vector>
#include <unordered_map>
//using namespace std;
//using namespace std::chrono;

typedef	struct {
    double    a;
    double    b;
    double    c;
} LineEquation;

double get_y(double x, LineEquation this_eq);
double dist_from_line(double x, double y, double a, double b, double c);
void linear_model_ransac(int* data, int size, int max_iter, double thresh, double min_inlier_perc, LineEquation* this_eq);

int calc_num_windows(int total_samples, int window_size, int overlap_size);
void hannInPlace(fftw_complex* v, int length, double* m);
std::size_t arrayHasher(int* thisArr);
void find_peaks(int* peak_freq, fftw_complex* out, int* subband_limits, int num_bands, int wind_size, int sampling_rate);

std::unordered_map<std::size_t, int> computeOrigFingerprint(AudioFile<double> audioFile, int sampling_rate, int wind_size, int overlap, fftw_complex* in, fftw_complex* out, fftw_plan plan, int* subband_limits, int num_bands);
void computeQueryFingerprint(AudioFile<double> audioFile, int sampling_rate, int wind_size, int overlap, fftw_complex* in, fftw_complex* out, fftw_plan plan, int* subband_limits, int num_bands, size_t* result);

void createAllOriginalAudioFingerprints(int num_fp);
void decodeOrigFingerprints(int num_fp, std::unordered_map<std::size_t, int> * original_fingerprints);
void shazam(std::string query, int num_fp, std::unordered_map<std::size_t, int>* original_fingerprints, int* final_video_prediction, double* final_second_prediction, int* final_frame_prediction);