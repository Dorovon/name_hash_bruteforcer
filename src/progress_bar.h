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
    size_t elapsed = std::chrono::duration_cast<std::chrono::seconds>( now - begin ).count();
    double progress = completed_combinations / total_combinations;
    auto r = hps( elapsed );
    util::print( "[{: >6.2f}%] {:02d}:{:02d}:{:02d} elapsed, {:.2f} {}h/s", progress * 100.0, elapsed / 3600, elapsed / 60 % 60, elapsed % 60, r.first, r.second );
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

  std::pair<double, std::string> hps( size_t elapsed_seconds ) const
  {
    double rate = completed_combinations / static_cast<double>( elapsed_seconds );
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
    size_t elapsed = std::chrono::duration_cast<std::chrono::seconds>( now - begin ).count();
    if ( static_cast<size_t>( elapsed ) < 1 )
      return;
    double progress = completed_combinations / total_combinations;
    size_t eta = static_cast<size_t>( elapsed / progress * ( 1.0 - progress ) );
    auto r = hps( elapsed );
    util::printr( "[{: >6.2f}%], {:02d}:{:02d}:{:02d} remaining, {:.2f} {}h/s", progress * 100.0, eta / 3600, eta / 60 % 60, eta % 60, r.first, r.second );
  }
};
