#include"Boss/Mod/ComplainerByLowSuccessPerDay.hpp"
#include"Boss/Msg/ProbeActively.hpp"
#include"Boss/Msg/RaisePeerComplaint.hpp"
#include"Boss/Msg/SolicitPeerComplaints.hpp"
#include"Boss/Msg/TimerRandomHourly.hpp"
#include"Boss/concurrent.hpp"
#include"Boss/log.hpp"
#include"Boss/random_engine.hpp"
#include"Ev/Io.hpp"
#include"S/Bus.hpp"
#include"Stats/ReservoirSampler.hpp"
#include"Util/make_unique.hpp"
#include"Util/stringify.hpp"
#include<set>

namespace {

/* Nodes with less than this success_per_day should be complained about.  */
auto constexpr min_success_per_day = double(0.35);
/* Nodes with less than or equal to this success_per_day should be actively
 * probed to try to boost that metric; maybe the metric is low only because
 * the random active probing rolled against it too often.
 */
auto constexpr warn_success_per_day = double(1.0);
/* Maximum number of nodes we will actively probe.  */
auto constexpr max_probes = std::size_t(10);

}

namespace Boss { namespace Mod {

class ComplainerByLowSuccessPerDay::Impl {
private:
	S::Bus& bus;

	/* Set of nodes that we need to probe actively because their
	 * success_per_day is fairly low.
	 */
	std::set<Ln::NodeId> pending_probe;

	void start() {
		bus.subscribe< Msg::SolicitPeerComplaints
			     >([this](Msg::SolicitPeerComplaints const& metrics) {
			return Ev::lift().then([this, metrics] () {
				auto act = Ev::lift();

				for (auto const& m : metrics.weeks2) {
					auto success_per_day = m.second.success_per_day;
					/* Mark those below the warning level.  */
					if (success_per_day <= warn_success_per_day)
						pending_probe.insert(m.first);

					if (success_per_day >= min_success_per_day)
						continue;

					auto reason = std::string()
						    + "ComplainerByLowSuccessPerDay: "
						    + "Payment attempts "
						    + "(probes + actual payments) "
						    + "that reach destination is only "
						    + Util::stringify(success_per_day)
						    + "/day, minimum is "
						    + Util::stringify(min_success_per_day)
						    + "/day"
						    ;

					act += bus.raise(Msg::RaisePeerComplaint{
						m.first,
						std::move(reason)
					});
				}

				return act;
			});
		});

		bus.subscribe< Msg::TimerRandomHourly
			     >([this](Msg::TimerRandomHourly const& _) {
			return Ev::lift().then([this]() {
				/* Probe only up to max_probes.  */
				auto sampler = Stats::ReservoirSampler<Ln::NodeId>(
					max_probes
				);
				for (auto const& node : pending_probe)
					sampler.add(node, 1.0, Boss::random_engine);
				auto selected = std::move(sampler).finalize();

				/* Trigger the active probes.  */
				auto act = Ev::lift();
				for (auto const& node : selected) {
					/* Remove from pending.  */
					auto it = pending_probe.find(node);
					pending_probe.erase(it);

					/* Raise the message.  */
					act += Boss::concurrent(bus.raise(Msg::ProbeActively{
						node
					}));
				}

				return act;
			});
		});
	}

public:
	Impl() =delete;
	Impl(Impl&&) =delete;
	Impl(Impl const&) =delete;

	explicit
	Impl(S::Bus& bus_) : bus(bus_) { start();  }
};

ComplainerByLowSuccessPerDay::~ComplainerByLowSuccessPerDay() =default;
ComplainerByLowSuccessPerDay::ComplainerByLowSuccessPerDay(ComplainerByLowSuccessPerDay&&)
	=default;

ComplainerByLowSuccessPerDay::ComplainerByLowSuccessPerDay(S::Bus& bus)
	: pimpl(Util::make_unique<Impl>(bus)) { }

}}
