

#include <catch/catch.hpp>

#include <vector>
#include <random>

#include <rmngr/resource/ioresource.hpp>
#include <rmngr/scheduler/scheduler.hpp>
#include <rmngr/scheduler/resource.hpp>
#include <rmngr/scheduler/dispatch.hpp>
#include <rmngr/scheduler/fifo.hpp>
#include <rmngr/scheduler/graphviz.hpp>

template <
    template <typename Job>
    typename JobSelector
>
struct StressTest
{
    std::default_random_engine generator;
    std::exponential_distribution<double> time_distribution;
    std::geometric_distribution<int> branch_distribution;
    std::bernoulli_distribution param_distribution;
    std::bernoulli_distribution result_distribution;
    int max_depth;

    struct TestData
        : rmngr::IOResource
    {
        TestData() :x(0){}
        int x;
    };

    std::vector< TestData > data;

    using Scheduler = rmngr::Scheduler<
        boost::mpl::vector<
            rmngr::ResourceUserPolicy,
	    rmngr::GraphvizWriter< typename rmngr::DispatchPolicy<JobSelector>::RuntimeProperty >,
	    rmngr::DispatchPolicy< JobSelector >
        >
    >;

    Scheduler scheduler;

    StressTest(
	int n_threads = 0,
	int n_resources = 10,
	int max_depth = 10,
	double lambda_time = 4.0,
	double p_branch = 0.4,
	double p_param = 0.5,
	double p_result = 0.5
    )
        : time_distribution( lambda_time )
	, branch_distribution( p_branch )
	, param_distribution( p_param )
	, result_distribution( p_result )
	, data( n_resources, TestData() )
	, scheduler( n_threads )
    {
        random_work(*this, 0, {});
    }

    void random_sleep( void )
    {
	std::chrono::duration<double> wait_period ( time_distribution(generator) );
	std::this_thread::sleep_for( wait_period );    
    }

  static void prop( rmngr::observer_ptr<typename Scheduler::Schedulable> s, StressTest& st, int depth, std::vector<std::reference_wrapper<TestData>> data )
    {
      /*
        auto & al = scheduler.proto_property< rmngr::ResourceUserPolicy >( s );
        for( auto& d : data )
            al.push_back( d.get().write() );
      */
    }

    static void random_work( StressTest& st, int depth, std::vector<std::reference_wrapper<TestData>> data )
    {
        static auto random_functor = st.scheduler.make_functor( &random_work, &prop );

        st.random_sleep();

	if( depth > st.max_depth )
	    return;

	int n_subfunctors = st.branch_distribution( st.generator );
	if( depth < 3 )
            n_subfunctors += 3 - depth;

	for(int i = 0; i < n_subfunctors; ++i )
	{
            std::vector<std::reference_wrapper<TestData>> args;
  	    for( auto& d : data )
	    {
                if( st.param_distribution( st.generator ) )
                    args.push_back( d );
	    }
            auto res = random_functor( std::ref(st), depth+1, args );

	    st.random_sleep();

	    if( st.result_distribution( st.generator ) )
                res.get();

	    st.random_sleep();
	}
    }
};

TEST_CASE("stress test")
{
    StressTest<rmngr::FIFO> st(10);
}

