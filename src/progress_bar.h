#pragma once

#include <chrono>
#include "util.h"

#include <iostream>
#include <mutex>
#include <string>
#include <string_view>

struct progress_bar_t
{
  double total_combinations;
  double completed_combinations;
  std::string output;
  std::chrono::steady_clock::time_point begin;
  int finished_threads;
  std::mutex mutex;

  progress_bar_t( double total ) :
    total_combinations( total ), completed_combinations(), output(), finished_threads(), mutex()
  {
    begin = std::chrono::steady_clock::now();
  }

  ~progress_bar_t()
  {
    std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
    size_t elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>( now - begin ).count();
    size_t elapsed_seconds = std::chrono::duration_cast<std::chrono::seconds>( now - begin ).count();
    if ( elapsed_ms == 0 )
      return;

    double progress;
    {
      std::lock_guard<std::mutex> guard( mutex );
      progress = completed_combinations / total_combinations;
    }
    auto r = hps( elapsed_ms );
    util::print( "[{: >6.2f}%] {:02d}:{:02d}:{:02d}.{:03d} elapsed, {:.2f} {}h/s", progress * 100.0,
      elapsed_seconds / 3600, elapsed_seconds / 60 % 60, elapsed_seconds % 60, elapsed_ms % 1000, r.first, r.second );
  }

  bool is_finished( int num_threads )
  {
    std::lock_guard<std::mutex> guard( mutex );
    return finished_threads >= num_threads;
  }

  void reset_threads()
  {
    std::lock_guard<std::mutex> guard( mutex );
    finished_threads = 0;
  }

  void finish_thread()
  {
    std::lock_guard<std::mutex> guard( mutex );
    finished_threads++;
  }

  void reset()
  {
    std::lock_guard<std::mutex> guard( mutex );
    begin = std::chrono::steady_clock::now();
    completed_combinations = 0.0;
    finished_threads = 0;
  }

  void finish()
  {
    std::lock_guard<std::mutex> guard( mutex );
    completed_combinations = total_combinations;
  }

  inline void increment( size_t value )
  {
    std::lock_guard<std::mutex> guard( mutex );
    completed_combinations += static_cast<double>( value );
  };

  std::pair<double, std::string> hps( size_t elapsed_ms )
  {
    if ( total_combinations == 0 || elapsed_ms == 0 )
      return { 0.0, "" };

    double rate;
    {
      std::lock_guard<std::mutex> guard( mutex );
      rate = completed_combinations / static_cast<double>( elapsed_ms ) * 1000.0;
    }

    std::string prefix = "";
    if ( rate > 1000000000 )
    {
      rate /= 1000000000;
      prefix = "G";
    }
    else if ( rate > 1000000 )
    {
      rate /= 1000000;
      prefix = "M";
    }
    else if ( rate > 1000 )
    {
      rate /= 1000;
      prefix = "K";
    }
    return { rate, prefix };
  }

  void out()
  {
    std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
    size_t elapsed_seconds = std::chrono::duration_cast<std::chrono::seconds>( now - begin ).count();
    size_t elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>( now - begin ).count();
    if ( elapsed_seconds < 1 )
      return;
    double progress;
    {
      std::lock_guard<std::mutex> guard( mutex );
      progress = completed_combinations / total_combinations;
    }
    size_t eta = static_cast<size_t>( elapsed_seconds / progress * ( 1.0 - progress ) );
    auto r = hps( elapsed_ms );
    util::printr( "[{: >6.2f}%] {:02d}:{:02d}:{:02d} remaining, {:.2f} {}h/s", progress * 100.0, eta / 3600, eta / 60 % 60, eta % 60, r.first, r.second );
  }
};
