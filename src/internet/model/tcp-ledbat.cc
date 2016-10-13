/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2016 NITK Surathkal
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Author: Ankit Deepak <adadeepak8@gmail.com>
 *
 */

#include "tcp-ledbat.h"
#include "ns3/tcp-socket-base.h"
#include "ns3/log.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("TcpLedbat");
NS_OBJECT_ENSURE_REGISTERED (TcpLedbat);

TypeId
TcpLedbat::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::TcpLedbat")
    .SetParent<TcpNewReno> ()
    .AddConstructor<TcpLedbat> ()
    .SetGroupName ("Internet")
    .AddAttribute ("TargetDelay",
                   "Targeted Queue Delay",
                   UintegerValue (100),
                   MakeUintegerAccessor (&TcpLedbat::m_Target),
                   MakeUintegerChecker<uint32_t> ())
    .AddAttribute ("baseHistoryLen",
                   "Number of Base delay Sample",
                   UintegerValue (10),
                   MakeUintegerAccessor (&TcpLedbat::m_baseHistoLen),
                   MakeUintegerChecker<uint32_t> ())
    .AddAttribute ("noiseFilterLen",
                   "Number of Current delay Sample",
                   UintegerValue (10),
                   MakeUintegerAccessor (&TcpLedbat::m_noiseFilterLen),
                   MakeUintegerChecker<uint32_t> ())
    .AddAttribute ("ledbatSSThresh",
                   "Slow Start Threshold",
                   UintegerValue (0xffff),
                   MakeUintegerAccessor (&TcpLedbat::m_ledbatSsthresh),
                   MakeUintegerChecker<uint32_t> ())
    .AddAttribute ("Gain",
                   "Offset Gain",
                   DoubleValue (1.0),
                   MakeDoubleAccessor (&TcpLedbat::m_gain),
                   MakeDoubleChecker<double> ())
    .AddAttribute ("SSParam",
                   "Possibility of Slow Start",
                   UintegerValue (1),
                   MakeUintegerAccessor (&TcpLedbat::SetDoSs),
                   MakeUintegerChecker<uint32_t> ())
  ;
  return tid;
}

void TcpLedbat::SetDoSs (uint32_t doSS)
{
  m_doSs = doSS;
  if (m_doSs)
    {
      m_flag |= LEDBAT_CAN_SS;
    }
  else
    {
      m_flag &= ~LEDBAT_CAN_SS;
    }
}

TcpLedbat::TcpLedbat (void)
  : TcpNewReno ()
{
  NS_LOG_FUNCTION (this);
  m_Target = 100;
  m_gain = 1;
  m_doSs = 1;
  m_ledbatSsthresh = 0xffff;
  m_baseHistoLen = 10;
  m_noiseFilterLen = 4;
  LedbatInitCircbuf (m_baseHistory);
  LedbatInitCircbuf (m_noiseFilter);
  m_lastRollover = 0;
  m_sndCwndCnt = 0;
  m_flag = LEDBAT_CAN_SS;
}

void TcpLedbat::LedbatInitCircbuf (struct OwdCircBuf &buffer)
{
  NS_LOG_FUNCTION (this);
  buffer.buffer.clear ();
  buffer.min = 0;
}

TcpLedbat::TcpLedbat (const TcpLedbat& sock)
  : TcpNewReno (sock)
{
  NS_LOG_FUNCTION (this);
  m_Target = sock.m_Target;
  m_gain = sock.m_gain;
  m_doSs = sock.m_doSs;
  m_baseHistoLen = sock.m_baseHistoLen;
  m_noiseFilterLen = sock.m_noiseFilterLen;
  m_ledbatSsthresh = sock.m_ledbatSsthresh;
  m_baseHistory = sock.m_baseHistory;
  m_noiseFilter = sock.m_noiseFilter;
  m_lastRollover = sock.m_lastRollover;
  m_sndCwndCnt = sock.m_sndCwndCnt;
  m_flag = sock.m_flag;
}

TcpLedbat::~TcpLedbat (void)
{
  NS_LOG_FUNCTION (this);
}

Ptr<TcpCongestionOps>
TcpLedbat::Fork (void)
{
  return CopyObject<TcpLedbat> (this);
}

std::string
TcpLedbat::GetName () const
{
  return "TcpLedbat";
}

uint32_t TcpLedbat::LedbatMinCircBuff (struct OwdCircBuf &b)
{
  if (b.buffer.size () == 0)
    {
      return ~0;
    }
  else
    {
      return b.buffer[b.min];
    }
}

uint32_t TcpLedbat::LedbatCurrentDelay (LedbatFilterFunction filter)
{
  return filter (m_noiseFilter);
}

uint32_t TcpLedbat::LedbatBaseDelay ()
{
  return LedbatMinCircBuff (m_baseHistory);
}

uint32_t TcpLedbat::GetSsThresh (Ptr<const TcpSocketState> tcb,
                                 uint32_t bytesInFlight)
{
  NS_LOG_FUNCTION (this << tcb << bytesInFlight);
  uint32_t res;
  switch (m_doSs)
    {
    case DO_NOT_SLOWSTART:
    case DO_SLOWSTART:
    default:
      res = TcpNewReno::GetSsThresh (tcb, bytesInFlight);
      break;
    case DO_SLOWSTART_WITH_THRESHOLD:
      res = m_ledbatSsthresh;
      break;
    }
  return res;
}

void TcpLedbat::IncreaseWindow (Ptr<TcpSocketState> tcb, uint32_t segmentsAcked)
{
  NS_LOG_FUNCTION (this << tcb << segmentsAcked);
  if (tcb->m_cWnd.Get () <= tcb->m_segmentSize)
    {
      m_flag |= LEDBAT_CAN_SS;
    }
  if (m_doSs >= DO_SLOWSTART && tcb->m_cWnd <= tcb->m_ssThresh && (m_flag & LEDBAT_CAN_SS))
    {
      SlowStart (tcb, segmentsAcked);
    }
  else
    {
      m_flag &= ~LEDBAT_CAN_SS;
      CongestionAvoidance (tcb, segmentsAcked);
    }
}

void TcpLedbat::CongestionAvoidance (Ptr<TcpSocketState> tcb, uint32_t segmentsAcked)
{
  NS_LOG_FUNCTION (this << tcb << segmentsAcked);
  int64_t queue_delay;
  int64_t offset;
  int64_t cwnd;
  uint32_t max_cwnd;
  int64_t current_delay;
  int64_t base_delay;

  max_cwnd = (tcb->m_cWnd.Get ()) * m_Target;
  current_delay = (int64_t)LedbatCurrentDelay (&TcpLedbat::LedbatMinCircBuff);
  base_delay = (int64_t)LedbatBaseDelay ();
  queue_delay = current_delay - base_delay;
  offset = ((int64_t)m_Target) - (queue_delay);
  offset *= m_gain;
  if (offset > m_Target)
    {
      offset = m_Target;
    }
  cwnd = m_sndCwndCnt + offset;
  if (cwnd >= 0)
    {
      m_sndCwndCnt = cwnd;
      if  (m_sndCwndCnt > max_cwnd)
        {
          tcb->m_cWnd += tcb->m_segmentSize;
          m_sndCwndCnt = 0;
        }
    }
  else
    {
      if (tcb->m_cWnd.Get () > tcb->m_segmentSize)
        {
          tcb->m_cWnd -= tcb->m_segmentSize;
          m_sndCwndCnt = (tcb->m_cWnd - tcb->m_segmentSize) * m_Target;
        }
    }
}

void TcpLedbat::LedbatAddDelay (struct OwdCircBuf &cb, uint32_t owd, uint32_t maxlen)
{
  if (cb.buffer.size () == 0)
    {
      cb.buffer.push_back (owd);
      cb.min = 0;
      return;
    }
  cb.buffer.push_back (owd);
  if (cb.buffer[cb.min] > owd)
    {
    }
  cb.min = cb.buffer.size () - 1;
  if (cb.buffer.size () == maxlen)
    {
      cb.buffer.erase (cb.buffer.begin ());
      cb.min = 0;
      for (uint32_t i = 1; i < maxlen - 1; i++)
        {
          if (cb.buffer[i] < cb.buffer[cb.min])
            {
              cb.min = i;
            }
        }
    }
}

void TcpLedbat::LedbatUpdateCurrentDelay (uint32_t owd)
{
  LedbatAddDelay (m_noiseFilter, owd, m_noiseFilterLen);
}

void TcpLedbat::LedbatUpdateBaseDelay (uint32_t owd)
{
  if (m_baseHistory.buffer.size () == 0)
    {
      LedbatAddDelay (m_baseHistory, owd, m_baseHistoLen);
      return;
    }
  uint64_t timestamp = (uint64_t) Simulator::Now ().GetSeconds ();

  if (timestamp - m_lastRollover > 60)
    {
      m_lastRollover = timestamp;
      LedbatAddDelay (m_baseHistory, owd, m_baseHistoLen);
    }
  else
    {
      uint32_t last = m_baseHistory.buffer.size () - 1;
      if (owd < m_baseHistory.buffer[last])
        {
          m_baseHistory.buffer[last] = owd;
          if (owd < m_baseHistory.buffer[m_baseHistory.min])
            {
              m_baseHistory.min = last;
            }
        }
    }
}

void TcpLedbat::PktsAcked (Ptr<TcpSocketState> tcb, uint32_t segmentsAcked,
                           const Time& rtt)
{
  NS_LOG_FUNCTION (this << tcb << segmentsAcked << rtt);
  if (rtt.IsPositive ())
    {
      LedbatUpdateCurrentDelay (tcb->m_rcvtsval - tcb->m_rcvtsecr);
      LedbatUpdateBaseDelay (tcb->m_rcvtsval - tcb->m_rcvtsecr);
    }
}
}
