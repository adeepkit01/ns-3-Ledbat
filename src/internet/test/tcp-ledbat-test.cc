#include "ns3/test.h"
#include "ns3/log.h"
#include "ns3/tcp-congestion-ops.h"
#include "ns3/tcp-socket-base.h"
#include "ns3/tcp-ledbat.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("TcpLedbatTestSuite");

/**
 * \brief Testing the congestion avoidance increment on TcpLedbat
 */
class TcpLedbatTest : public TestCase
{
public:
  TcpLedbatTest (uint32_t cWnd, uint32_t segmentSize, uint32_t ssThresh,
                 uint32_t segmentsAcked, SequenceNumber32 highTxMark,
                 SequenceNumber32 lastAckedSeq, Time rtt, const std::string &name);

private:
  virtual void DoRun (void);
  void IncreaseWindow (Ptr<TcpLedbat> cong);

  uint32_t m_cWnd;
  uint32_t m_segmentSize;
  uint32_t m_segmentsAcked;
  uint32_t m_ssThresh;
  Time m_rtt;
  SequenceNumber32 m_highTxMark;
  SequenceNumber32 m_lastAckedSeq;
  Ptr<TcpSocketState> m_state;
};

TcpLedbatTest::TcpLedbatTest (uint32_t cWnd, uint32_t segmentSize, uint32_t ssThresh,
                              uint32_t segmentsAcked, SequenceNumber32 highTxMark,
                              SequenceNumber32 lastAckedSeq, Time rtt, const std::string &name)
  : TestCase (name),
    m_cWnd (cWnd),
    m_segmentSize (segmentSize),
    m_segmentsAcked (segmentsAcked),
    m_ssThresh (ssThresh),
    m_rtt (rtt),
    m_highTxMark (highTxMark),
    m_lastAckedSeq (lastAckedSeq)
{
}

void
TcpLedbatTest::DoRun ()
{
  m_state = CreateObject <TcpSocketState> ();
  m_state->m_cWnd = m_cWnd;
  m_state->m_ssThresh = m_ssThresh;
  m_state->m_segmentSize = m_segmentSize;
  m_state->m_highTxMark = m_highTxMark;
  m_state->m_lastAckedSeq = m_lastAckedSeq;

  Ptr<TcpLedbat> cong = CreateObject <TcpLedbat> ();
  cong->SetAttribute ("SSParam", UintegerValue (0));
  cong->SetAttribute ("noiseFilterLen", UintegerValue (1));

  m_state->m_rcvtsval = 2;
  m_state->m_rcvtsecr = 1;
  cong->PktsAcked (m_state, m_segmentsAcked, m_rtt);

  m_state->m_rcvtsval = 7;
  m_state->m_rcvtsecr = 4;
  cong->PktsAcked (m_state, m_segmentsAcked, m_rtt);

  cong->IncreaseWindow (m_state, m_segmentsAcked);

  IncreaseWindow (cong);

  NS_TEST_ASSERT_MSG_EQ (m_state->m_cWnd.Get (), m_cWnd,
                         "CWnd has not updated correctly");
}

void TcpLedbatTest::IncreaseWindow (Ptr<TcpLedbat> cong)
{
  UintegerValue target;
  cong->GetAttribute ("TargetDelay", target);
  NS_TEST_ASSERT_MSG_EQ (target.Get (), 100, "target delay fine");
  DoubleValue gain;
  cong->GetAttribute ("Gain", gain);
  NS_TEST_ASSERT_MSG_EQ (gain.Get (), 1.0, "gain fine");
  uint32_t queue_delay = 2;
  int64_t offset = (((int64_t)target.Get ()) - (queue_delay));
  offset *= gain.Get ();
  int32_t sndCwndCnt = offset * m_segmentsAcked * m_segmentSize;
  uint32_t cwnd;
  uint32_t absSndCnt = sndCwndCnt > 0 ? (uint32_t)sndCwndCnt : (uint32_t)(-1 * sndCwndCnt);
  if (absSndCnt >= m_cWnd * target.Get ())
    {
      int32_t inc =  (sndCwndCnt) / (target.Get () * m_cWnd);
      cwnd += inc * m_segmentSize;
      sndCwndCnt -= inc * m_cWnd * target.Get ();
    }

  uint32_t max_cwnd = (m_highTxMark - m_lastAckedSeq) + m_segmentsAcked * m_segmentSize;
  cwnd = std::min (cwnd, max_cwnd);
  cwnd = std::max (cwnd, m_segmentSize);
  m_cWnd = cwnd;
}

static class TcpLedbatTestSuite : public TestSuite
{
public:
  TcpLedbatTestSuite () : TestSuite ("tcp-ledbat-test", UNIT)
  {
    AddTestCase (new TcpLedbatTest (3 * 1446, 1446, 2 * 1446, 2, SequenceNumber32 (3753), SequenceNumber32 (3216), MilliSeconds (100), "TCP Ledbat Test"), TestCase::QUICK);
  }
} g_tcpledbatTest;

}
