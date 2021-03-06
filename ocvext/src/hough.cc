
/** @file   hough.cc
 *  @brief  Hough transform / using gradient
 *  @author J.A. / 2015 / www.tsdconseil.fr
 *  @license LGPL V 3.0 */

#include "hough.hpp"
#include "gl.hpp"
#include <cmath>
#include <iostream>
#include "opencv2/imgproc/imgproc.hpp"
#include "opencv2/highgui/highgui.hpp"

#ifndef M_PI
# define M_PI 3.1415926
#endif

/** Hough transform using gradient matrix */
template<typename Tin>
void hough_gradient_template(const cv::Mat &g_abs,
                    const cv::Mat &g_angle,
                    cv::Mat &res,
                    float rho_res, float theta_res)
{
  int ntheta = (int) ceil(2 * M_PI / theta_res);
  uint16_t sx = g_abs.cols, sy = g_abs.rows;
  int rmax = (int) std::ceil(std::sqrt(sx*sx+sy*sy) / rho_res);
  cv::Mat acc = cv::Mat::zeros(ntheta, rmax, CV_32F);

  for(int y = 0; y < sy; y++)
  {
    for(int x = 0; x < sx; x++)
    {
      float gnorm = (float) g_abs.at<Tin>(y,x);
      float theta = (float) g_angle.at<Tin>(y,x);

      int rho = (int) std::round((x * std::cos(theta) + y * std::sin(theta)) / rho_res);
      if(rho < 0)
      {
        rho = -rho;
        theta = theta + M_PI;
        if(theta > 2 * M_PI)
          theta -= 2 * M_PI;
      }
      if(rho >= rmax)
        rho = rmax - 1;

      int ti = std::floor((ntheta * theta) / (2 * M_PI));
      acc.at<float>(ti, rho) += (float) gnorm;
    } // i
  } //j
  cv::normalize(acc, res, 0, 255.0, cv::NORM_MINMAX);
}


/** Hough transform using gradient matrix */
template<typename Tin>
void hough_no_gradient_template(const cv::Mat &gradient_abs,
                       cv::Mat &res,
                       float rho_res, float theta_res)
{
  unsigned int ntheta = (int) ceil(M_PI / theta_res);
  uint16_t sx = gradient_abs.cols, sy = gradient_abs.rows;
  int i, j;
  // CEIL
  int cx = (sx >> 1) + (sx & 1);
  int cy = (sy >> 1) + (sy & 1);
  int rmax = (int) std::ceil(std::sqrt(sx*sx+sy*sy)/2);
  signed short rho;
  unsigned int nrho = 2*rmax/rho_res+1;

  res = cv::Mat::zeros(cv::Size(ntheta, nrho), CV_32F);

  for(j = 0; j < sy; j++)
  {
    int y = j - cy;

    const float *gptr = gradient_abs.ptr<float>(j);

    for(i = 0; i < sx; i++)
    {
      int x = i - cx;
      float gnorm = *gptr++;//gradient_abs.at<Tin>(i,j);
      for(auto k = 0u; k < (unsigned int) ntheta; k++)
      {
        float theta = (k * M_PI) / ntheta;
        rho = (int) std::round((x * std::cos(theta) + y * std::sin(theta)) / rho_res);
        int ri = (rmax / rho_res) + rho;
        if(ri >= (int) nrho)
          ri = nrho -1;
        if(ri < 0)
          ri = 0;
        res.at<float>(k,ri) += gnorm;
      }
    } // i
    printf("j = %d.\n", j); fflush(0);
  } //j

  cv::normalize(res, res, 0, 255.0, cv::NORM_MINMAX);
}


void HoughWithGradientDir(const cv::Mat &g_abs,
                          const cv::Mat &g_angle,
                          cv::Mat &res,
                          float rho, float theta)
{
  auto type = g_abs.depth();
  switch(type)
  {
  case CV_32F:
    hough_gradient_template<float>(g_abs, g_angle, res, rho, theta);
    break;
  case CV_16S:
    hough_gradient_template<int16_t>(g_abs, g_angle, res, rho, theta);
    break;
  default:
    std::cerr << "hough_gradient: type non supporté (" << type << ")." << std::endl;
    CV_Assert(0);
  }
}

void HoughWithGradientDir(const cv::Mat &img,
                          cv::Mat &res,
                          float rho_res,
                          float theta_res,
                          float gamma)
{
  cv::Mat gris = img, gx, gy, mag, angle;
  if(img.channels() != 1)
    cv::cvtColor(img, gris, CV_BGR2GRAY);
  DericheGradient(gris, gx, gy, gamma);
  cv::cartToPolar(gx, gy, mag, angle);
  cv::normalize(mag, mag, 0, 1.0, cv::NORM_MINMAX);
  HoughWithGradientDir(mag, angle, res, rho_res, theta_res);
}

void HoughWithoutGradientDir(const cv::Mat &img,
                             cv::Mat &res,
                             float rho_res,
                             float theta_res,
                             float gamma)
{
  cv::Mat gris = img, gx, gy, mag, angle;
  if(img.channels() != 1)
    cv::cvtColor(img, gris, CV_BGR2GRAY);
  printf("deriche...\n"); fflush(0);
  DericheGradient(gris, gx, gy, gamma);
  cv::cartToPolar(gx, gy, mag, angle);
  cv::normalize(mag, mag, 0, 1.0, cv::NORM_MINMAX);
  printf("template...\n"); fflush(0);
  hough_no_gradient_template<float>(mag, res, rho_res, theta_res);
  printf("ok.\n"); fflush(0);
}

/** Top hat filter (band-pass) approximation using Deriche
 *  exponential filter */
static void my_top_hat(const cv::Mat &I, cv::Mat &O,
                       float g1, float g2)
{
  cv::Mat If1, If2;
  DericheBlur(I, If1, g1);
  DericheBlur(I, If2, g2);
  O = cv::abs(If1 - If2); // Passe bas (haute fréq) - Passe bas (basse fréq)
}


#define dbgsave(XX,YY)
/*static void dbgsave(const std::string &name, const cv::Mat &I)
{
  cv::Mat O;
  cv::normalize(I, O, 0, 255.0, cv::NORM_MINMAX);
  cv::imwrite(name, O);
}*/


void HoughLinesWithGradientDir(const cv::Mat &img,
                               std::vector<cv::Vec2f> &lines,
                               float rho_res,
                               float theta_res,
                               float gamma)
{
  cv::Mat prm, mask, mask2, mask3;
  dbgsave("build/0-entree.png", img);
  HoughWithGradientDir(img, prm, rho_res, theta_res, gamma);
  dbgsave("build/1-hough.png", prm);
  my_top_hat(prm, prm, 0.2, 0.6);
  cv::normalize(prm, prm, 0, 1.0, cv::NORM_MINMAX);
  dbgsave("build/2-tophat.png", prm);
  cv::threshold(prm, prm, 0.4, 0, cv::THRESH_TOZERO);
  dbgsave("build/3-seuil.png", prm);
  cv::Mat K = cv::getStructuringElement(cv::MORPH_RECT,
                                        cv::Size(7, 7),
                                        cv::Point(1, 1));

  cv::Mat mask_max, mask_min;
  cv::dilate(prm, mask_max, K);
  cv::erode(prm, mask_min, K);
  // On veut >= max dans le voisinage
  // Mais aussi > au min dans le voisinage
  cv::compare(prm, mask_max, mask2, cv::CMP_GE);
  cv::compare(prm, mask_min, mask3, cv::CMP_GT);
  mask2 &= mask3;
  dbgsave("build/4-locmax.png", mask2);

  cv::Mat locations;   // output, locations of non-zero pixels
  cv::findNonZero(mask2, locations);

  for(auto i = 0u; i < locations.total(); i++)
  {
    int ri = locations.at<cv::Point>(i).x;
    int ti = locations.at<cv::Point>(i).y;
    float theta = ti * theta_res;
    float rho = ri;
    cv::Vec2f line(rho, theta);
    lines.push_back(line);
  }
}




