/** @file GEMHwDevice.h */

#ifndef GEM_HW_GEMHWDEVICE_H
#define GEM_HW_GEMHWDEVICE_H

#include <iomanip>
#include <memory>

/* #include "xdata/String.h" */
/* #include "xdata/UnsignedLong.h" */
/* #include "xdata/UnsignedInteger32.h" */
// can we decouple completely from these?
#include "toolbox/string.h"
/* #include "toolbox/mem/Reference.h" */
/* #include "toolbox/mem/Pool.h" */

#include "uhal/uhal.hpp"
/* #include "uhal/Utilities.hpp"  /\* removed in 2.5 or 2.6 *\/ */
#include "uhal/utilities/bits.hpp"
#include "uhal/utilities/files.hpp"

#include "xhal/XHALInterface.h"
/* #include "xhal/XHALDevice.h" */

#include "gem/hw/utils/GEMCrateUtils.h"
#include "gem/hw/exception/Exception.h"

/// TODO: would be good to decouple from utils, maybe migrate RegisterUtils to gem::hw::utils
#include "gem/utils/GEMLogging.h"
#include "gem/utils/GEMRegisterUtils.h"
// TODO: This is a generically useful lock, both inside and outside of the HW, but is it used in the HW yet?
#include "gem/utils/Lock.h"
#include "gem/utils/LockGuard.h"

typedef uhal::exception::exception uhalException;

// for multiple reads with single dispatch with named registers
typedef std::pair<std::string, uint32_t> register_pair;
typedef std::vector<register_pair>       register_pair_list;

// for multiple reads with single dispatch with addressed registers
typedef std::pair<uint32_t, uint32_t>           addressed_register_pair;
typedef std::vector<addressed_register_pair>    addressed_register_pair_list;

// for multiple reads with single dispatch with addressed and masked registers
typedef std::pair<std::pair<uint32_t, uint32_t>, uint32_t> masked_register_pair;
typedef std::vector<masked_register_pair>                  masked_register_pair_list;

typedef std::pair<std::string, uhal::ValWord<uint32_t> > register_value;
typedef std::vector<register_value>                      register_val_list;


/* namespace uhal { */
/*   class HwInterface; */
/* } */

/* namespace xhal { */
/*   class XHALInterface; */
/* } */

namespace gem {
  namespace hw {

    class GEMHwDevice : public xhal::XHALInterface, public uhal::HwInterface
    {

    public:
      /* IPBus transactions still have some problems in the firmware
         so it helps to retry a few times in the case of a failure
         that is recognized
      */
      static const unsigned MAX_IPBUS_RETRIES = 5;

      /** TODO: REMOVE
       * @struct OpticalLinkStatus
       * @brief This structure stores retrieved counters related to the optical links
       * @var OpticalLinkStatus::GTX_TRK_Errors
       * GTX_TRK_Errors is a counter for the number of errors on the 8b10b tracking data link
       * @var OpticalLinkStatus::GTX_TRG_Errors
       * GTX_TRG_Errors is a counter for the number of errors on the 8b10b trigger data link
       * @var OpticalLinkStatus::GTX_Data_Packets
       * GTX_Data_Packets is a counter for the number of data packets transferred on the 8b10b tracking data link
       * @var OpticalLinkStatus::GBT_TRK_Errors
       * GBT_TRK_Errors is a counter for the number of errors on the GBT tracking data link
       * @var OpticalLinkStatus::GBT_Data_Packets
       * GBT_Data_Packets is a counter for the number of data packets transferred on the GBT tracking data link
       */
      typedef struct OpticalLinkStatus {
        uint32_t GTX_TRK_Errors  ;
        uint32_t GTX_TRG_Errors  ;
        uint32_t GTX_Data_Packets;
        uint32_t GBT_TRK_Errors  ;
        uint32_t GBT_Data_Packets;

      OpticalLinkStatus() :
        GTX_TRK_Errors(0),
          GTX_TRG_Errors(0),
          GTX_Data_Packets(0),
          GBT_TRK_Errors(0),
          GBT_Data_Packets(0) {};

        void reset() {
          GTX_TRK_Errors=0;
          GTX_TRG_Errors=0;
          GTX_Data_Packets=0;
          GBT_TRK_Errors=0;
          GBT_Data_Packets=0;
          return; };
      } OpticalLinkStatus;

      /** TODO: REMOVE or REDESIGN
       * @struct DeviceErrors
       * @brief This structure stores retrieved counters related to the IPBus transaction errors
       * @var DeviceErrors::BadHeader
       * BadHeader is a counter for the number times the IPBus transaction returned a bad header
       * @var DeviceErrors::ReadError
       * ReadError is a counter for the number read transaction errors
       * @var DeviceErrors::Timeout
       * Timeout is a counter for the number for the number of timeouts
       * @var DeviceErrors::ControlHubErr
       * ControlHubErr is a counter for the number control hub errors encountered
       */
      typedef struct DeviceErrors {
        int BadHeader    ;
        int ReadError    ;
        int Timeout      ;
        int ControlHubErr;

      DeviceErrors() : BadHeader(0),ReadError(0),Timeout(0),ControlHubErr(0) {};
        void reset() { BadHeader=0; ReadError=0; Timeout=0; ControlHubErr=0; return; };
      } DeviceErrors;

      // TODO: REDESIGN
      typedef std::pair<uint8_t, OpticalLinkStatus>  linkStatus;
      //typedef std::vector<linkStatus>                linkStatus;

      /**
       * @brief GEMHwDevice constructor based on hostname and connection file
       *
       * @param deviceName card name (connection file and IP resolveable)
       * @param connectionFile name of the connection file to find the `uhal` device endpoint
       */
      GEMHwDevice(std::string const& deviceName,
                  std::string const& connectionFile);

      /**
       * @brief GEMHwDevice constructor based on hostname, URI, and address table
       *
       * @param deviceName card name (connection file and IP resolveable)
       * @param connectionURI `uhal` device endpoint
       * @param addressTable address table to be used with `uhal` register access
       */
      GEMHwDevice(std::string const& deviceName,
                  std::string const& connectionURI,
                  std::string const& addressTable);

      /**
       * @brief GEMHwDevice constructor based on hostname and existing uhal::HwInterface
       *
       * @param deviceName card name (connection file and IP resolveable)
       * @param uhalDevice already created uhal::HwInterface to use for connection
       */
      GEMHwDevice(std::string       const& deviceName,
                  uhal::HwInterface const& uhalDevice);

      /** TODO: IMPLEMENT
       * @brief GEMHwDevice constructor based on shelf, slot (with optional parameters to set up the uhal connection)
       * @description This constructor will take only physical properties of the card and create the URI, using a default address table
       *              The URI has several necessary components:
       *                - protocol: ipbusudp, ipbustcp, chtcp
       *                - port: default for ipbusNative is 50001, otherwise 60002
       *                - chHost: if using a control hub, the host on which the control hub is running
       *                - cardname: gem-shelf<shelf>-amc<slot>
       *              The URI will be constructed as follows:
       *                - chtcp-2.0:://<chHost>:<10203>?target=<cardName>:<port>, for a controlhub connection
       *                - <protocol>:://<cardName>:<port>, for a standalone ipbus connection (TCP:ipbustcp-2.0 or UDP: ipbusudp-2.0)
       *
       * @param shelf shelfID of the device connection point
       * @param slot  AMC slot in a uTCA shelf (blade in an ATCA crate)
       * @param ipbusNative, will be false for any Zynq or similar type devices
       * @param chHost, only possible for ipbusNative devices, the control hub hostname
       * /
      GEMHwDevice(uint8_t     const& shelf,
                  uint8_t     const& shelf,
                  bool        const& ipbusNative=false,
                  std::string const& chHost="");
      */
      virtual ~GEMHwDevice();

      /* FIXME REMOVE virtual bool isHwConnected() { return p_gemHW != 0; }; */
      virtual bool isHwConnected() { return true; }; /*FIXME make pure virutal, non-virtual?*/

      ///////////////////////////////////////////////////////////////////////////////////////
      //****************Methods implemented for convenience on uhal devices****************//
      ///////////////////////////////////////////////////////////////////////////////////////

      /**
       * @defgroup uhalwrappers Generic read/write wrapper functions on uhal/IPBus devices.
       * The operations will be the same for the GLIB, CTP7, MP7, VFAT2/3, OptoHybrid2/3 and AMC13
       * (we should use the already defined AMC13, rather than write our own,
       * unless there are GEM specific functions that need to be implemented)
       */

      /**
       * @ingroup uhalwrappers
       * @brief read from a register identified by a register name
       *
       * @usage
       *
       * @param regName name of the register to read
       *
       * @retval returns the 32 bit unsigned value in the register
       */
      uint32_t readReg(std::string const& regName);

      /**
       * @ingroup uhalwrappers
       * @brief read from a register identified by a raw address
       *
       * @usage
       *
       * @param regAddr address of the register to read
       *
       * @retval returns the 32 bit unsigned value in the register
       */
      uint32_t readReg(uint32_t const& regAddr);

      /**
       * @ingroup uhalwrappers
       * @brief read from a register identified by an address, with the supplied mask
       *
       * @usage
       *
       * @param regAddr address of the register to read
       * @param regMask mask of the register to read
       *
       * @retval returns the 32 bit unsigned value in the register
       */
      uint32_t readReg(uint32_t const& regAddr, uint32_t const& regMask);

      /**
       * @ingroup uhalwrappers
       * @brief read from a register identified by prefix.name in the address table
       *
       * @usage
       *
       * @param regPrefix prefix in the address table, possibly root nodes
       * @param regName name of the register to read from the address table
       *
       * @retval returns the 32 bit unsigned value
       */
      uint32_t readReg(const std::string &regPrefix, const std::string &regName) { return readReg(regPrefix+"."+regName); };

      /**
       * @ingroup uhalwrappers
       * @brief read from a register identified by prefix.name in the address table
       *        FIXME REDUNDANT
       *
       * @usage This function is redundant, if the address table knows there is a mask applied
       *        it will be used when calculating the return type
       *
       * @param regName name of the register to read
       *
       * @retval returns the 32 bit unsigned value in the register
       */
      uint32_t readMaskedAddress(std::string const& regName);

      /**
       * @ingroup uhalwrappers
       * @brief read list of registers in a single transaction (one dispatch call)
       *        into the supplied vector regList
       *
       * @usage Return values are stored in the regList object passed in as a parameter
       *
       * @param regList list of register name and uint32_t value to store the result
       * @param freq integer number of transactions to bundle (-1 for all)
       */
      void     readRegs(register_pair_list &regList, int const& freq=8);

      /**
       * @ingroup uhalwrappers
       * @brief read list of registers in a single transaction (one dispatch call)
       *        into the supplied vector regList
       *
       * @usage
       *
       * @param regList list of register address and uint32_t value to store the result
       * @param freq integer number of transactions to bundle (-1 for all)
       */
      void     readRegs(addressed_register_pair_list &regList, int const& freq=8);

      /**
       * @ingroup uhalwrappers
       * @brief read list of registers in a single transaction (one dispatch call)
       *        into the supplied vector regList
       *
       * @usage
       *
       * @param regList list of register address/mask pair and uint32_t value to store the result
       * @param freq integer number of transactions to bundle (-1 for all)
       */
      void     readRegs(masked_register_pair_list &regList, int const& freq=8);

      /**
       * @ingroup uhalwrappers
       * @brief write to a register identified by a register name
       *
       * @usage
       *
       * @param regName name of the register to read
       * @param val value to write to the register
       */
      void     writeReg(std::string const& regName, uint32_t const val);

      /**
       * @ingroup uhalwrappers
       * @brief write to a register identified by a raw address
       *
       * @usage
       *
       * @param regAddr address of the register to read
       * @param val value to write to the register
       */
      void     writeReg(uint32_t const& regAddr, uint32_t const val);

      /**
       * @ingroup uhalwrappers
       * @brief write to a register identified by prefix.name in the address table
       *
       * @usage
       *
       * @param regPrefix prefix in the address table to the register
       * @param regName name of the register to write to
       * @param val value to write to the register
       */
      void     writeReg(const std::string &regPrefix, const std::string &regName, uint32_t const val) { return writeReg(regPrefix+"."+regName, val); };

      /**
       * @ingroup uhalwrappers
       * @brief write list of registers in a single transaction (one dispatch call)
       *        using the supplied vector regList
       *
       * @usage
       *
       * @param regList std::vector of a pairs of register names and values to write
       * @param freq integer number of transactions to bundle (-1 for all)
       */
      void     writeRegs(register_pair_list const& regList, int const& freq=8);

      /**
       * @ingroup uhalwrappers
       * @brief write single value to a list of registers in a single transaction
       *        (one dispatch call) using the supplied vector regList
       *
       * @usage
       *
       * @param regList list of registers to write a value to
       * @param regValue uint32_t value to write to the list of registers
       * @param freq integer number of transactions to bundle (-1 for all)
       */
      void     writeValueToRegs(std::vector<std::string> const& regList, uint32_t const& regValue, int const& freq=8);

      /**
       * @ingroup uhalwrappers
       * @brief write zero to a single register
       *
       * @usage
       *
       * @param regName register to zero
       */
      void     zeroReg( std::string const& regName) { writeReg(regName,0); };

      /**
       * @ingroup uhalwrappers
       * @brief write zero to a list of registers in a single transaction (one dispatch call)
       *        using the supplied vector regNames
       *
       * @usage
       *
       * @param regNames list of registers to zero
       */
      void     zeroRegs(std::vector<std::string> const& regNames, int const& freq=8);

      /**
       * @ingroup uhalwrappers
       * @brief read from a memory block
       *
       * @usage
       *
       * @param regName fixed size memory block to read from
       */
      std::vector<uint32_t> readBlock(std::string const& regName);

      /**
       * @ingroup uhalwrappers
       * @brief read from a memory block
       *
       * @usage
       *
       * @param regName name of memory block to read from
       * @param nWords size of the memory block to read
       *
       * @retval a vector of 32 bit unsigned values
       */
      std::vector<uint32_t> readBlock(std::string const& regName, size_t const& nWords);

      /**
       * @ingroup uhalwrappers
       * @brief read from a memory block
       *
       * @usage
       *
       * @param regName name of memory block to read from
       * @param buffer a pointer to an array containing the read values
       * @param nWords size of the memory block to read
       *
       * @retval the size of the returned array
       */
      uint32_t readBlock(std::string const& regName, uint32_t* buffer, size_t const& nWords);

      /* / ** */
      /*  * @ingroup uhalwrappers */
      /*  * @brief read from a memory block */
      /*  * */
      /*  * @usage */
      /*  * */
      /*  * @param regName name of memory block to read from */
      /*  * @param buffer a vector of pointers to memory locations containing the read values */
      /*  * @param nWords size of the memory block to read */
      /*  * */
      /*  * @retval the size of the returned array */
      /*  * / */
      /* uint32_t readBlock(std::string const& regName, std::vector<toolbox::mem::Reference*>& buffer, */
      /*                    size_t const& nWords); */

      /**
       * @ingroup uhalwrappers
       * @brief write to a memory block
       *
       * @usage
       *
       * @param regName name of memory block to write to
       * @param values list of 32-bit words to write into the memory block
       */
      void writeBlock(std::string const& regName, std::vector<uint32_t> const values);

      /**
       * @ingroup uhalwrappers
       * @brief write zeros to a block of memory
       *
       * @usage
       *
       * @param regName block or memory to zero
       */
      void zeroBlock(std::string const& regName);

      /**
       * @ingroup uhalwrappers FIXME, isn't this a block, not a FIFO?
       * @brief readFIFO(std::string const& regName) read from a FIFO/memory port
       *
       * @usage
       *
       * @param regName fixed size memory block to read from
       */
      std::vector<uint32_t> readFIFO(std::string const& regName);
      //size_t readFIFO(std::string const& regName, size_t nWords, uint32_t* buffer); /*hcal style */

      /**
       * @ingroup uhalwrappers
       * @brief read from a FIFO/memory port a fixed amount of data
       *
       * @usage
       *
       * @param regName FIFO to read from
       * @param nWords number of words to read from the FIFO
       *
       * @retval a vector of 32 bit unsigned values
       */
      std::vector<uint32_t> readFIFO(std::string const& regName, size_t const& nWords);

      /**
       * @ingroup uhalwrappers
       * @brief write to a FIFO
       *
       * @usage
       *
       * @param regName FIFO to write to
       * @param values list of 32-bit words to write into the FIFO
       */
      void writeFIFO(std::string const& regName, std::vector<uint32_t> const values);

      /**
       * @ingroup uhalwrappers FIXME: does this even make semantic sense?
       * @brief reset a FIFO
       *
       * @usage
       *
       * @param regName FIFO to zero
       */
      void zeroFIFO(std::string const& regName);


      // These methods provide access to the member variables
      // specifying the `uhal` address table name and the IPbus protocol
      // version.
      //getters
      const std::string getControlHubIPAddress()  const { return m_controlHubIPAddress;};
      const std::string getDeviceIPAddress()      const { return m_deviceIPAddress;    };
      const std::string getIPBusProtocolVersion() const { return m_ipBusProtocol;      };
      const std::string getAddressTableFileName() const { return m_addressTable;       };

      const std::string getDeviceBaseNode()       const { return m_deviceBaseNode; };
      const std::string getDeviceID()             const { return m_deviceID;       };

      const uint32_t getControlHubPort() const { return m_controlHubPort;};
      const uint32_t getIPBusPort()      const { return m_ipBusPort;     };

      //setters, should maybe be private/protected? defeats the purpose?
      void setControlHubIPAddress( std::string const& ipAddress) { m_controlHubIPAddress = ipAddress; };
      void setIPBusProtocolVersion(std::string const& version) { m_ipBusProtocol = version; };
      void setDeviceIPAddress(     std::string const& deviceIPAddr) { m_deviceIPAddress = deviceIPAddr; };
      void setAddressTableFileName(std::string const& name) { m_addressTable = "file://${GEM_ADDRESS_TABLE_PATH}/"+name; };

      void setDeviceBaseNode(std::string const& deviceBase) { m_deviceBaseNode = deviceBase; };
      void setDeviceID(      std::string const& deviceID) { m_deviceID = deviceID; };

      void setControlHubPort(uint32_t const& port) { m_controlHubPort = port; };
      void setIPBusPort(     uint32_t const& port) { m_ipBusPort = port; };

      /* uhal::HwInterface& getGEMHwInterface() const; */


      ///////////////////////////////////////////////////////////////////////////////////////
      //******************* Generic properties of the GEMHwDevice object ******************//
      ///////////////////////////////////////////////////////////////////////////////////////

      std::string getLoggerName() const { return m_gemLogger.getName(); };

      void updateErrorCounters(std::string const& errCode);

      virtual std::string printErrorCounts() const;

      /**
       * @brief performs a general reset of the GLIB
       *
       * @usage
       *
       */
      virtual void generalReset();

      /**
       * @brief performs a reset of the GLIB counters
       *
       * @usage
       *
       */
      virtual void counterReset();

      /**
       * @brief performs a reset of the GLIB link
       *
       * @usage
       *
       * @param link is the link to perform the reset on
       */
      virtual void linkReset(uint8_t const& link);

      DeviceErrors m_ipBusErrs;

      bool b_is_connected;

    protected:
      /* std::shared_ptr<uhal::ConnectionManager> p_gemConnectionManager; */
      /* std::shared_ptr<uhal::HwInterface> p_gemHW; */

      log4cplus::Logger m_gemLogger;

      mutable gem::utils::Lock m_hwLock;

      /**
       * @brief Performs basic setup for the device
       * sets connection details (OBSOLETE)
       * sets logging level to Error
       * Not inherited
       **/
      void setup(std::string const& deviceName);

    private:
      // Do Not use default constructor. GEMHwDevice object should only be made using
      // either connection file method or with a list of URIs and Address Tables
      GEMHwDevice();

      // Prevent copying of GEMHwDevice objects
      GEMHwDevice( const GEMHwDevice& other) ; // prevents construction-copy
      GEMHwDevice& operator=( const GEMHwDevice&) ; // prevents copying

      /**
       * @brief loads the necessary modules into the RPC module manager
       * @details pure virtual, must be implemented in derived classes
       *
       * @usage
       *
       * @param should reconnect in the case of an already connected manager
       */
      virtual void connectRPC(bool reconnect=false)=0;

      std::string m_controlHubIPAddress;  //!<
      std::string m_addressTable;         //!<
      std::string m_ipBusProtocol;        //!<
      std::string m_deviceIPAddress;      //!<

      std::string m_deviceBaseNode;       //!<
      std::string m_deviceID;             //!<

      uint32_t m_controlHubPort;          //!<
      uint32_t m_ipBusPort;               //!<

      /** FIXME removed when infospace was dropped
      //infospace im(ex)portables
      xdata::String xs_controlHubIPAddress;  //!<
      xdata::String xs_deviceIPAddress;      //!<
      xdata::String xs_ipBusProtocol;        //!<
      xdata::String xs_addressTable;         //!<

      xdata::UnsignedInteger32 xs_controlHubPort;  //!<
      xdata::UnsignedInteger32 xs_ipBusPort;       //!<
      **/

      bool knownErrorCode(std::string const& errCode) const;
      //std::string registerToChar(uint32_t value) const;
    };  // class GEMHwDevice
  }  // namespace gem::hw
}  // namespace gem


/* // define the consts */
/* const unsigned gem::hw::GEMHwDevice::MAX_IPBUS_RETRIES; */

#endif  // GEM_HW_GEMHWDEVICE_H
