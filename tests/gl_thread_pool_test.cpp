#include <doctest/doctest.h>

import std;
import GLThreadPool;

// Asana 1220330336212700: Doodads::create and Units::create left at the first future that threw, while the other tasks kept
// using the Map that Open Map then deleted. wait_for_all must let every task finish before the error leaves.
TEST_CASE("wait_for_all waits for every task before rethrowing the first error") {
	std::atomic<int> finished = 0;
	std::vector<std::future<void>> futures;
	futures.push_back(std::async(std::launch::async, [] {
		throw std::runtime_error("first");
	}));
	for (int i = 0; i < 4; i++) {
		futures.push_back(std::async(std::launch::async, [&finished] {
			std::this_thread::sleep_for(std::chrono::milliseconds(100));
			finished++;
		}));
	}
	futures.push_back(std::async(std::launch::async, [] {
		std::this_thread::sleep_for(std::chrono::milliseconds(50));
		throw std::runtime_error("second");
	}));

	try {
		wait_for_all(futures);
		FAIL("wait_for_all did not throw");
	} catch (const std::runtime_error& error) {
		CHECK(std::string(error.what()) == "first");
	}
	CHECK(finished.load() == 4);
	for (const auto& future : futures) {
		CHECK_FALSE(future.valid()); // every result was taken
	}
}

TEST_CASE("wait_for_all returns normally when no task throws") {
	std::atomic<int> finished = 0;
	std::vector<std::future<void>> futures;
	for (int i = 0; i < 3; i++) {
		futures.push_back(std::async(std::launch::async, [&finished] { finished++; }));
	}
	CHECK_NOTHROW(wait_for_all(futures));
	CHECK(finished.load() == 3);
}
