#include <catch2/catch_test_macros.hpp>

#include "Geometry.h"

#include <memory>

using namespace fccvis::geometry;

namespace {

class TestGeometry : public AbstractShape {
public:
    explicit TestGeometry(double offset = 0.0)
        : offset(offset)
    {
    }

    double Criterion(const Vector& p) const override
    {
        return p[0] + p[1] + p[2] - offset;
    }

    Mesh Tessellate(double tolerance) const override
    {
        Mesh mesh;

        if (tolerance > 0.0) {
            mesh.vertices = {
                {0.0f, 0.0f, 0.0f},
                {1.0f, 0.0f, 0.0f},
                {0.0f, 1.0f, 0.0f}
            };

            mesh.triangles = {
                {0, 1, 2}
            };
        }

        return mesh;
    }

    std::shared_ptr<AbstractShape> Clone() const override
    {
        return std::make_shared<TestGeometry>(*this);
    }

private:
    double offset;
};

} // namespace

TEST_CASE("AbstractShape evaluates its criterion", "[geometry][abstract]")
{
    TestGeometry geometry;

    SECTION("negative criterion")
    {
        INFO("Point should produce a negative criterion");
        REQUIRE(geometry.Criterion({-1.0, 0.0, 0.0}) < 0.0);
    }

    SECTION("zero criterion")
    {
        INFO("Zero criterion represents the boundary");
        REQUIRE(geometry.Criterion({0.0, 0.0, 0.0}) == 0.0);
    }

    SECTION("positive criterion")
    {
        INFO("Point should produce a positive criterion");
        REQUIRE(geometry.Criterion({1.0, 0.0, 0.0}) > 0.0);
    }
}

TEST_CASE("AbstractShape can be used polymorphically",
          "[geometry][abstract]")
{
    std::shared_ptr<AbstractShape> geometry =
        std::make_shared<TestGeometry>();

    INFO("The concrete geometry must be accessible through AbstractShape");
    REQUIRE(geometry != nullptr);

    REQUIRE(geometry->Criterion({-1.0, 0.0, 0.0}) < 0.0);
    REQUIRE(geometry->Criterion({1.0, 0.0, 0.0}) > 0.0);
}

TEST_CASE("AbstractShape can be cloned", "[geometry][abstract]")
{
    TestGeometry geometry;

    auto clone = geometry.Clone();

    REQUIRE(clone != nullptr);
    REQUIRE(clone.get() != &geometry);

    INFO("Clone must preserve the geometric criterion");

    REQUIRE(clone->Criterion({-1.0, 2.0, 0.0}) ==
            geometry.Criterion({-1.0, 2.0, 0.0}));
}

TEST_CASE("AbstractShape tessellation returns a mesh",
          "[geometry][abstract]")
{
    TestGeometry geometry;

    SECTION("positive tolerance")
    {
        auto mesh = geometry.Tessellate(0.01);

        INFO("Expected one triangle");
        REQUIRE(mesh.vertices.size() == 3);
        REQUIRE(mesh.triangles.size() == 1);

        REQUIRE(mesh.triangles[0] == std::array<uint32_t, 3>{0, 1, 2});
    }

    SECTION("non-positive tolerance")
    {
        auto mesh = geometry.Tessellate(0.0);

        REQUIRE(mesh.vertices.empty());
        REQUIRE(mesh.triangles.empty());
    }
}
