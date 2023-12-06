#include "AudioFP.h"

double get_y(double x, LineEquation this_eq) {
    if (this_eq.a == 0 && this_eq.b == 0 && this_eq.c == 0) {
        return -1;
    }
    return (-this_eq.c - (this_eq.a * x)) / this_eq.b;
}

double dist_from_line(double x, double y, double a, double b, double c) {
    return abs(a * x + b * y + c) / sqrt(pow(a, 2) + pow(b, 2));
}
void linear_model_ransac(int* data, int size, int max_iter, double thresh, double min_inlier_perc, LineEquation* this_eq) {
    /*
    data – A set of observations.
    size - the size of the data
    max_iter – The maximum number of iterations allowed in the algorithm.
    thresh – A threshold value to determine data points that are fit well by the model (inlier).
    */

    //Randomize the seed
    srand(time(NULL));

    //Create params
    double best_a = 0;
    double best_b = 0;
    double best_c = 0;
    double best_err = DBL_MAX;

    //Get nonzero data
    int num_nonzero = 0;
    std::vector<int> nonzero_idx;
    //unordered_map<int, int> nonzero_map;
    for (int i = 0; i < size; i++) {
        if (data[i] > 0) {
            num_nonzero++;
            //nonzero_map[i] = data[i];
            nonzero_idx.push_back(i);
        }
    }

    //Minimum number of matches required to be valid
    if (num_nonzero >= 5) {
        int min_inliers = static_cast<int>(floor(min_inlier_perc * num_nonzero));

        //Perform RANSAC
        for (int iter = 0; iter < max_iter; iter++) {
            int idx1 = nonzero_idx[rand() % num_nonzero];
            int idx2 = nonzero_idx[rand() % num_nonzero];
            int val1 = data[idx1];
            int val2 = data[idx2];
            double slope = (static_cast<double>(val1) - val2) / (static_cast<double>(idx1) - idx2);
            double y_inter = val1 - slope * idx1;

            double this_a = -slope;
            double this_b = 1;
            double this_c = -y_inter;

            std::vector<int> confirmed_inliers;
            int num_inliers = 0;
            for (int i = 0; i < num_nonzero; i++) {
                int this_x = nonzero_idx[i];
                int this_y = data[this_x];
                double dist = dist_from_line(this_x, this_y, this_a, this_b, this_c);
                if (dist < thresh) {
                    confirmed_inliers.push_back(this_x);
                    num_inliers++;
                }
            }

            if (num_inliers > min_inliers) {
                double this_err = 0;
                for (int i = 0; i < num_inliers; i++) {
                    int this_x = confirmed_inliers[i];
                    int this_y = data[this_x];
                    this_err += dist_from_line(this_x, this_y, this_a, this_b, this_c);
                }
                if (this_err < best_err) {
                    best_a = this_a;
                    best_b = this_b;
                    best_c = this_c;

                    best_err = this_err;
                }
            }
        }

    }

    

    this_eq->a = best_a;
    this_eq->b = best_b;
    this_eq->c = best_c;
}




int calc_num_windows(int total_samples, int window_size, int overlap_size) {
    return static_cast<int>(floor((total_samples - overlap_size) / (window_size - overlap_size)));
}


void hannInPlace(fftw_complex* v, int length, double* m) {
    for (int i = 0; i < length; i++) {
        v[i][0] = m[i] * v[i][0];
    }
}

std::size_t arrayHasher(int* thisArr) {
    std::size_t h = 0;

    for (int i = 0; i < 5; i++) {
        h ^= std::hash<int>{}(thisArr[i]) + 0x9e3779b9 + (h << 6) + (h >> 2);
    }
    return h;
}

void find_peaks(int* peak_freq, fftw_complex* out, int* subband_limits, int num_bands, int wind_size, int sampling_rate) {
    for (int i = 0; i < num_bands; i++) {
        int upper_idx = 1 + (subband_limits[i] * wind_size / sampling_rate);
        int lower_idx;
        if (i == 0) {
            lower_idx = 1;
        }
        else {
            lower_idx = 1 + (subband_limits[i - 1] * wind_size / sampling_rate);
        }

        double max_val = 0;
        for (int idx = lower_idx; idx < upper_idx; idx++) {
            double magnitude = abs(out[idx][0]);
            if (magnitude > max_val) {
                max_val = magnitude;
                peak_freq[i] = idx;
            }
        }
    }
}

std::unordered_map<std::size_t, int> computeOrigFingerprint(AudioFile<double> audioFile, int sampling_rate, int wind_size, int overlap, fftw_complex* in, fftw_complex* out, fftw_plan plan, int* subband_limits, int num_bands) {
    /* Creates a fingerprint of an original audio file.
    Returns a dictionary, where keys are frequencies and
    values are timestamps*/
    std::unordered_map<std::size_t, int> thisMap;

    int num_samples = audioFile.getNumSamplesPerChannel();
    int num_windows = static_cast<int>(floor((num_samples - overlap) / (wind_size - overlap)));
    int channel = 0;

    double* m = (double*)malloc(sizeof(double) * wind_size);
    //Setup for hann function
    for (int i = 0; i < wind_size; i++) {
        m[i] = 0.5 * (1 - cos(2 * M_PI * i / wind_size));
    }

    //Slide a window across the audio file. For each window, perform some fourier analysis
    for (int wind_count = 0; wind_count < num_windows; wind_count++) {

        //Copy window data into buffer
        int start = wind_count * (wind_size - overlap);
        int start_milli = start / 44.1;
        for (int i = 0; i < wind_size; i++) {
            int start_idx = start + i;
            in[i][0] = audioFile.samples[channel][start_idx];
            in[i][1] = 0;
        }

        //Pass through hanning window
        hannInPlace(in, wind_size, m);

        //Perform fft
        fftw_execute(plan);

        //Find peaks within subbands
        int* peak_freq = (int*)malloc(sizeof(int) * num_bands);
        find_peaks(peak_freq, out, subband_limits, num_bands, wind_size, sampling_rate);

        //Hash the array of peaks
        std::size_t this_key = arrayHasher(peak_freq);

        //Store in dictionary
        thisMap[this_key] = start_milli;
        //printf("%d: %d %d %d %d %d => %d  \n", wind_count, peak_freq[0], peak_freq[1], peak_freq[2], peak_freq[3], peak_freq[4], start_milli);
    }
    return thisMap;
}

void computeQueryFingerprint(AudioFile<double> audioFile, int sampling_rate, int wind_size, int overlap, fftw_complex* in, fftw_complex* out, fftw_plan plan, int* subband_limits, int num_bands, size_t* result) {
    /* Creates a fingerprint of an query audio file.
        Searches for the matching original file*/
    int num_samples = audioFile.getNumSamplesPerChannel();
    int num_windows = static_cast<int>(floor((num_samples - overlap) / (wind_size - overlap)));
    int channel = 0;
    double* m = (double*)malloc(sizeof(double) * wind_size);
    //printf("numsamples: %d numwind: %d overlap: %d wind_size: %d\n", num_samples, num_windows, overlap, wind_size);
    //Setup for hann function
    for (int i = 0; i < wind_size; i++) {
        m[i] = 0.5 * (1 - cos(2 * M_PI * i / wind_size));
    }

    //Slide a window across the audio file. For each window, perform some fourier analysis
    for (int wind_count = 0; wind_count < num_windows; wind_count++) {

        //Copy window data into buffer
        int start = wind_count * (wind_size - overlap);
        for (int i = 0; i < wind_size; i++) {
            int start_idx = start + i;
            in[i][0] = audioFile.samples[channel][start_idx];
            in[i][1] = 0;
        }

        //Pass through hanning window
        hannInPlace(in, wind_size, m);

        //Perform fft
        fftw_execute(plan);

        //Find peaks within subbands
        int* peak_freq = (int*)malloc(sizeof(int) * num_bands);
        find_peaks(peak_freq, out, subband_limits, num_bands, wind_size, sampling_rate);

        //Hash the array of peaks
        std::size_t this_key = arrayHasher(peak_freq);
        result[wind_count] = this_key;

        //printf("%d: %d %d %d %d %d\n", wind_count, peak_freq[0], peak_freq[1], peak_freq[2], peak_freq[3], peak_freq[4]);
        //printf("%d, ", origMaps[0][this_key]);
    }
}