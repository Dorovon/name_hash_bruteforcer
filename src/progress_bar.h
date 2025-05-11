#pragma once

#include <chrono>
#include "util.h"

#include <atomic>
#include <iostream>
#include <string>
#include <string_view>

struct progress_bar_t
{
  double total_combinations;
  std::atomic<double> completed_combinations;
  std::string output;
  std::chrono::steady_clock::time_point begin;
  std::atomic<int> finished_threads;

  progress_bar_t( double total ) :
    total_combinations( total ), completed_combinations(), output(), finished_threads()
  {
    begin = std::chrono::steady_clock::now();
  }

  ~progress_bar_t()
  {
    std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
    size_t elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>( now - begin ).count();
    size_t elapsed_seconds = std::chrono::duration_cast<std::chrono::seconds>( now - begin ).count();
    if ( elapsed_seconds == 0 )
      return;

    double progress = completed_combinations / total_combinations;
    auto r = hps( elapsed_ms );
    util::print( "[{: >6.2f}%] {:02d}:{:02d}:{:02d}.{:03d} elapsed, {:.2f} {}h/s", progress * 100.0,
      elapsed_seconds / 3600, elapsed_seconds / 60 % 60, elapsed_seconds % 60, elapsed_ms % 1000, r.first, r.second );
  }

  void finish_thread()
  {
    finished_threads++;
  }

  void reset()
  {
    begin = std::chrono::steady_clock::now();
    completed_combinations = 0.0;
    finished_threads = 0;
  }

  void finish()
  {
    completed_combinations = total_combinations;
  }

  inline void increment( size_t value )
  {
    completed_combinations += static_cast<double>( value );
  };

  std::pair<double, std::string> hps( size_t elapsed_ms ) const
  {
    if ( total_combinations == 0 || elapsed_ms == 0 )
      return { 0.0, "" };

    double rate = completed_combinations / static_cast<double>( elapsed_ms ) * 1000.0;
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
    double progress = completed_combinations / total_combinations;
    size_t eta = static_cast<size_t>( elapsed_seconds / progress * ( 1.0 - progress ) );
    auto r = hps( elapsed_ms );
    util::printr( "[{: >6.2f}%], {:02d}:{:02d}:{:02d} remaining, {:.2f} {}h/s", progress * 100.0, eta / 3600, eta / 60 % 60, eta % 60, r.first, r.second );
  }
};
