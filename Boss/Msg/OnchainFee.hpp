#ifndef BOSS_MSG_ONCHAINFEE_HPP
#define BOSS_MSG_ONCHAINFEE_HPP

namespace Boss { namespace Msg {

/** struct Boss::Msg::OnchainFee
 *
 * @brief emitted at startup, as well as periodically,
 * and reporting whether or not fees are low.
 */
struct OnchainFee {
	bool fees_low;
};

}}

#endif /* !defined(BOSS_MSG_ONCHAINFEE_HPP) */
