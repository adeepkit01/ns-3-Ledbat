#ifndef TCPLEDBAT_H
#define TCPLEDBAT_H

#include "ns3/tcp-congestion-ops.h"
#include <vector>
#include "ns3/event-id.h"

namespace ns3 {


struct OwdCircBuf
{
  std::vector<uint32_t> buffer;
  uint32_t min;
};


/**
 * \ingroup congestionOps
 *
 * \brief An implementation of LEDBAT
 */

class TcpLedbat : public TcpNewReno
{
public:
  /**
   * \brief Get the type ID.
   * \return the object TypeId
   */
  static TypeId GetTypeId (void);

  /**
   * Create an unbound tcp socket.
   */
  TcpLedbat (void);

  /**
   * \brief Copy constructor
   * \param sock the object to copy
   */
  TcpLedbat (const TcpLedbat& sock);
  virtual ~TcpLedbat (void);

  virtual std::string GetName () const;

  virtual void PktsAcked (Ptr<TcpSocketState> tcb, uint32_t segmentsAcked,
                          const Time& rtt);

  virtual uint32_t GetSsThresh (Ptr<const TcpSocketState> tcb,
                                uint32_t bytesInFlight);

  virtual Ptr<TcpCongestionOps> Fork ();

  virtual void IncreaseWindow (Ptr<TcpSocketState> tcb, uint32_t segmentsAcked);

  void SetDoSs (uint32_t doSS);

protected:
  virtual void CongestionAvoidance (Ptr<TcpSocketState> tcb, uint32_t segmentsAcked);

private:
  uint32_t m_Target;
  double m_gain;
  uint32_t m_doSs;
  uint32_t m_ledbatSsthresh;
  uint32_t m_baseHistoLen;
  uint32_t m_noiseFilterLen;

  enum Ledbat_ss
  {
    DO_NOT_SLOWSTART,
    DO_SLOWSTART,
    DO_SLOWSTART_WITH_THRESHOLD
  };

  enum TcpLedbatState
  {
    LEDBAT_INCREASING = (1 << 2),
    LEDBAT_CAN_SS     = (1 << 3)
  };

  uint64_t m_lastRollover;
  uint32_t m_sndCwndCnt;
  struct OwdCircBuf m_baseHistory;
  struct OwdCircBuf m_noiseFilter;
  uint32_t m_flag;

  void LedbatInitCircbuf (struct OwdCircBuf &buffer);

  typedef uint32_t (*LedbatFilterFunction)(struct OwdCircBuf &);
  static uint32_t LedbatMinCircBuff (struct OwdCircBuf &b);
  uint32_t LedbatCurrentDelay (LedbatFilterFunction filter);
  uint32_t LedbatBaseDelay ();
  void LedbatAddDelay (struct OwdCircBuf &cb, uint32_t owd, uint32_t maxlen);
  void LedbatUpdateCurrentDelay (uint32_t owd);
  void LedbatUpdateBaseDelay (uint32_t owd);
};

}

#endif
