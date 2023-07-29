#include <stdint.h>
#include <vector>
#include <stdexcept>

double dot_product(const double *a, const double *b, const uint32_t n) {
  double ret = 0.0;
  for (uint32_t i = 0; i < n; i++) ret += (*(a + i)) * (*(b + i));
  return ret;
}

class NLMS_base {
        uint32_t n;
        double mu;
        double eps;
        std::vector<double> w;

        public:
        NLMS_base(const uint32_t n_, const double mu_ = 0.1, const double eps_ = 1.0);
        void adapt(const double true_val, const double *arr);
        double predict(const double *arr);
};

NLMS_base::NLMS_base(const uint32_t n_, const double mu_, const double eps_) {
  n = n_;
  mu = mu_;
  eps = eps_;
  if ((mu >= 1000.0 || mu <= 0.0) || (eps >= 1000.0 || eps <= 0.0))
    throw std::runtime_error("Invalid value of mu or eps.");
  w.resize(n, 0.0);
}

void NLMS_base::adapt(const double true_val, const double *arr) {
  double y = dot_product(&w[0], arr, n);
  double e = true_val - y;
  double nu = mu / (eps + dot_product(arr, arr, n));
  for (uint32_t i = 0; i < n; i++) w[i] += nu * e * (*(arr + i));
}

double NLMS_base::predict(const double *arr) {
  return dot_product(arr, &w[0], n);
}

class NLMS_predictor {
        NLMS_base *filter;
        uint32_t n;

public:
        NLMS_predictor(const uint32_t n_, const double mu);
        double predict(const std::vector<double> &recon_arr, const uint32_t idx);
        ~NLMS_predictor() { delete filter; }
};

NLMS_predictor::NLMS_predictor(const uint32_t n_, const double mu) {
        filter = new NLMS_base(n_, mu);
        n = n_;
}

double NLMS_predictor::predict(const std::vector<double> &recon_arr,
                              const uint32_t idx) {
        if (idx > n) {
                filter->adapt(recon_arr[idx - 1], &recon_arr[idx - n - 1]);
                return filter->predict(&recon_arr[idx - n]);
        } else if (idx > 0 && idx <= n) {
                return recon_arr[idx - 1];
        } else {
                return 0.0;
        }
}
