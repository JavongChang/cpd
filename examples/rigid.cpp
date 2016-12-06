/// Computes the rigid transformation between two text files.

#include <fstream>
#include <iostream>

#include "docopt.h"
#include <cpd/comparer/base.hpp>
#include <cpd/logging.hpp>
#include <cpd/rigid.hpp>
#include <cpd/runner.hpp>
#include <cpd/utils.hpp>
#include <cpd/vendor/spdlog/spdlog.h>

static const char USAGE[] =
    R"(Rigid cpd on two text files.

Usage:
    rigid <fixed> <moving> [--outfile=<filename>] [--sigma2=<n>] [--comparer=<name>]
    rigid (-h | --help)

Options:
    -h --help               Show this screen.
    --outfile=<filename>    The file to write the resultant points out to.
    --sigma2=<n>            The initial value for sigma2 [default: 0.0].
    --comparer=<name>       The name of the comparer to use [default: direct].
)";

int main(int argc, char** argv) {
    auto logger = spdlog::stdout_logger_mt(cpd::LOGGER_NAME);
    std::map<std::string, docopt::value> args =
        docopt::docopt(USAGE, { argv + 1, argv + argc }, true, "CPD rigid");
    cpd::Matrix fixed = cpd::matrix_from_path(args.at("<fixed>").asString());
    cpd::Matrix moving = cpd::matrix_from_path(args.at("<moving>").asString());
    double sigma2 = std::stod(args.at("--sigma2").asString());
    auto comparer = cpd::Comparer::from_name(args.at("--comparer").asString());
    cpd::Rigid rigid;
    cpd::Runner<cpd::Rigid> runner(rigid);
    runner.sigma2(sigma2).comparer(std::move(comparer));
    cpd::Rigid::Result result = runner.run(fixed, moving);
    std::cout << result << std::endl;
    std::cout << "Average translation: "
              << (result.points - moving).colwise().mean() << std::endl;

    if (args.at("--outfile")) {
        std::ofstream f(args.at("--outfile").asString());
        f << result.points;
    }
    return 0;
}
