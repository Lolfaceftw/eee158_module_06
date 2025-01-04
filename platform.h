/**
 * @file  platform.h
 * @brief Declarations for platform-support routines
 *
 * @author Alberto de Villa <alberto.de.villa@eee.upd.edu.ph>
 * @date   28 Oct 2024
 */

#if !defined(EEE158_EX05_PLATFORM_H_) 
#define EEE158_EX05_PLATFORM_H_

#include <stdbool.h>
#include <stddef.h>
#include <limits.h>
#include <stdint.h>

// C linkage should be maintained
#ifdef __cplusplus
extern "C" {
#endif

    /// Initialize the platform, including any hardware peripherals.
    void platform_init(void);

    /**
     * Do one loop of events processing for the platform
     * 
     * @note
     * This is expected to be called within the main application infinite loop.
     */
    void platform_do_loop_one(void);

    //////////////////////////////////////////////////////////////////////////////

    /// Pushbutton event mask for pressing the on-board button
#define PLATFORM_PB_ONBOARD_PRESS	0x0001

    /// Pushbutton event mask for releasing the on-board button
#define PLATFORM_PB_ONBOARD_RELEASE	0x0002

    /// Pushbutton event mask for the on-board button
#define PLATFORM_PB_ONBOARD_MASK	(PLATFORM_PB_ONBOARD_PRESS | PLATFORM_PB_ONBOARD_RELEASE)

    /**
     * Determine which pushbutton events have pressed since this function was last
     * called
     * 
     * @return	A bitmask of @code PLATFORM_PB_* @endcode values denoting which
     * 	        event/s occurred
     */
    uint16_t platform_pb_get_event(void);

    //////////////////////////////////////////////////////////////////////////////

    /// Indefinitely dim
#define PLATFORM_BLINK_OFF	(0)

    /// Slow blink
#define PLATFORM_BLINK_SLOW	(1)

    /// Medium-paced blink
#define PLATFORM_BLINK_MEDIUM	(2)

    /// Fast blink
#define PLATFORM_BLINK_FAST	(3)

    /// Indefinitely lit
#define PLATFORM_BLINK_ON	(4)

    /**
     * Modify the blinking state
     * 
     * @param[in]	val	New blinking state
     */
    void platform_blink_modify();

    //////////////////////////////////////////////////////////////////////////////

    /**
     * Structure representing a time specification
     * 
     * @note
     * This is inspired by @code struct timespec @endcode used within the Linux
     * kernel and syscall APIs, but is not intended to be compatible with either
     * set of APIs.
     */
    typedef struct platform_timespec_type {
        /// Number of seconds elapsed since some epoch
        uint32_t nr_sec;

        /**
         * Number of nanoseconds
         * 
         * @note
         * Routines expect this value to lie on the interval [0, 999999999].
         */
        uint32_t nr_nsec;
    } platform_timespec_t;

    /// Initialize a @c timespec structure to zero
#define PLATFORM_TIMESPEC_ZERO {0, 0}

    /**
     * Compare two timespec instances
     * 
     * @param[in]	lhs	Left-hand side
     * @param[in]	rhs	Right-hand side
     * 
     * @return -1 if @c lhs is earlier than @c rhs, +1 if @c lhs is later than
     *         @c rhs, zero otherwise
     */
    int platform_timespec_compare(const platform_timespec_t *lhs,
            const platform_timespec_t *rhs);

    /// Number of microseconds for a single tick
#define	PLATFORM_TICK_PERIOD_US	5000

    /// Return the number of ticks since @c platform_init() was called
    void platform_tick_count(platform_timespec_t *tick);

    /**
     * A higher-resolution version of @c platform_tick_count(), if available
     * 
     * @note
     * If unavailable, this function is equivalent to @c platform_tick_count().
     */
    void platform_tick_hrcount(platform_timespec_t *tick);

    /**
     * Get the difference between two ticks
     * 
     * @note
     * This routine accounts for wrap-arounds, but only once.
     * 
     * @param[out]	diff	Difference
     * @param[in]	lhs	Left-hand side
     * @param[in]	rhs	Right-hand side
     */
    void platform_tick_delta(
            platform_timespec_t *diff,
            const platform_timespec_t *lhs, const platform_timespec_t *rhs
            );

    //////////////////////////////////////////////////////////////////////////////

    /// Descriptor for reception via USART

    typedef struct platform_usart_rx_desc_type {
        /// Buffer to store received data into
        char *buf;

        /// Maximum number of bytes for @c buf
        uint16_t max_len;

        /// Type of completion that has occurred
        volatile uint16_t compl_type;

        /// No reception-completion event has occurred
#define PLATFORM_USART_RX_COMPL_NONE	0x0000

        /// Reception completed with a received packet
#define PLATFORM_USART_RX_COMPL_DATA	0x0001

        /**
         * Reception completed with a line break
         * 
         * @note
         * This completion event is not implemented in this sample.
         */
#define PLATFORM_USART_RX_COMPL_BREAK	0x0002

        /// Extra information about a completion event, if applicable

        volatile union {
            /**
             * Number of bytes that were received
             * 
             * @note
             * This member is valid only if @code compl_type == PLATFORM_USART_RX_COMPL_DATA @endcode.
             */
            uint16_t data_len;
        } compl_info;
    } platform_usart_rx_async_desc_t;

    /// Descriptor for a transmission fragment

    typedef struct platform_usart_tx_desc_type {
        /// Start of the buffer to transmit
        const char *buf;

        /// Size of the buffer
        uint16_t len;
    } platform_usart_tx_bufdesc_t;

    /**
     * Enqueue an array of fragments for transmission
     * 
     * @note
     * All fragment-array elements and source buffer/s must remain valid for the
     * entire time transmission is on-going.
     * 
     * @p	desc	Descriptor array
     * @p	nr_desc	Number of descriptors
     * 
     * @return	@c true if the transmission is successfully enqueued, @c false
     *		otherwise
     */
    bool platform_usart_cdc_tx_async(const platform_usart_tx_bufdesc_t *desc,
            unsigned int nr_desc);

    /// Abort an ongoing transmission
    void platform_usart_cdc_tx_abort(void);

    /// Check whether a transmission is on-going
    bool platform_usart_cdc_tx_busy(void);

    /**
     * Enqueue a request for data reception
     * 
     * @note
     * Both descriptor and target buffer must remain valid for the entire time
     * reception is on-going.
     * 
     * @p	desc	Descriptor
     * 
     * @return	@c true if the reception is successfully enqueued, @c false
     *		otherwise
     */
    bool platform_usart_cdc_rx_async(platform_usart_rx_async_desc_t *desc);

    /// Abort an ongoing transmission
    void platform_usart_cdc_rx_abort(void);

    /// Check whether a reception is on-going
    bool platform_usart_cdc_rx_busy(void);

    //////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif	// __cplusplus
#endif	// !defined(EEE158_EX05_PLATFORM_H_)