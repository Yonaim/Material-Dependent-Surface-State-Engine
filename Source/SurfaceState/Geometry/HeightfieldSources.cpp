/**
 * @file HeightfieldSources.cpp
 * @brief 서로 다른 형상 입력을 동일한 표면 차트로 샘플링하고 높이 대역을 분리한다.
 */

#include "SurfaceState/Geometry/HeightfieldSources.h"
#include "AssetManager/Loaders/OBJLoader.h"
#include "AssetManager/Loaders/TextureLoader.h"
#include "SurfaceState/Simulation/SurfaceSimulationDomain.h"
#include <algorithm>
#include <bit>
#include <cmath>
#include <complex>
#include <limits>
#include <mutex>
#include <numbers>
#include <stdexcept>

namespace MDSS::SurfaceState
{
    // ============================================================
    // 00. Geometry Sampling Helpers
    // ============================================================
    #pragma region 00_Geometry_Sampling_Helpers
        // 서로 다른 형상 입력을 로컬 차트의 높이 표본으로 맞춘다.

        namespace
        {
            constexpr std::array<std::array<int, 2>, 4> Neighbors{{{-1, 0}, {1, 0}, {0, -1}, {0, 1}}};

            template <class FReadSample>
            auto SampleImage(std::uint32_t Width, std::uint32_t Height, float U, float V, FReadSample Read)
            {
                const float X = std::clamp(U * Width - 0.5F, 0.F, static_cast<float>(Width - 1));
                const float Y = std::clamp((1.F - V) * Height - 0.5F, 0.F, static_cast<float>(Height - 1));
                const auto  LoX = static_cast<std::uint32_t>(X);
                const auto  LoY = static_cast<std::uint32_t>(Y);
                const auto  HiX = std::min(LoX + 1, Width - 1);
                const auto  HiY = std::min(LoY + 1, Height - 1);
                return glm::mix(glm::mix(Read(LoX, LoY), Read(HiX, LoY), X - LoX),
                                glm::mix(Read(LoX, HiY), Read(HiX, HiY), X - LoX),
                                Y - LoY);
            }

            glm::vec3 texel(const Asset::FTextureData &Image, float U, float V)
            {
                return SampleImage(Image.Width,
                                   Image.Height,
                                   U,
                                   V,
                                   [&](std::uint32_t Column, std::uint32_t Row)
                                   {
                                       const auto Value = &Image.Pixels[4ULL * (Row * Image.Width + Column)];
                                       return glm::vec3(Value[0], Value[1], Value[2]) / 255.F;
                                   });
            }

            void SampleHeightMap(const std::filesystem::path &Path, Liquid::FHeightfield &Shape, float Scale)
            {
                const auto Image = Asset::FTextureLoader::LoadHeightMap(Path);
                const auto Read = [&](std::uint32_t Column, std::uint32_t Row)
                { return Image.Samples[static_cast<std::size_t>(Row) * Image.Width + Column] / 65535.F; };
                for (int Y = 0; Y < Shape.Rows; ++Y)
                    for (int X = 0; X < Shape.Columns; ++X)
                        Shape.Heights[Y * Shape.Columns + X] = Scale * SampleImage(Image.Width,
                                                                                   Image.Height,
                                                                                   (X + 0.5F) / Shape.Columns,
                                                                                   (Y + 0.5F) / Shape.Rows,
                                                                                   Read);
            }

            using FComplex = std::complex<double>;
            constexpr double Pi = std::numbers::pi_v<double>;

            void FourierPowerOfTwo(std::span<FComplex> Values, bool bInverse = false)
            {
                const auto Size = Values.size();
                for (std::size_t Index = 1, Reversed = 0; Index < Size; ++Index)
                {
                    auto Bit = Size >> 1;
                    while ((Reversed & Bit) != 0)
                    {
                        Reversed ^= Bit;
                        Bit >>= 1;
                    }
                    Reversed ^= Bit;
                    if (Index < Reversed)
                        std::swap(Values[Index], Values[Reversed]);
                }
                for (std::size_t Span = 2; Span <= Size; Span <<= 1)
                {
                    const auto Step = std::polar(1.0, (bInverse ? 2.0 : -2.0) * Pi / Span);
                    for (std::size_t Base = 0; Base < Size; Base += Span)
                    {
                        auto Phase = FComplex{1.0};
                        for (std::size_t Index = 0; Index < Span / 2; ++Index)
                        {
                            auto      &Even = Values[Base + Index];
                            auto      &Odd = Values[Base + Index + Span / 2];
                            const auto Shifted = Odd * Phase;
                            Odd = Even - Shifted;
                            Even += Shifted;
                            Phase *= Step;
                        }
                    }
                }
                if (bInverse)
                    for (auto &Value : Values)
                        Value /= static_cast<double>(Size);
            }

            class FCosineTransform
            {
                public:
                explicit FCosineTransform(int Count)
                    : Spectrum(2 * Count),
                      Rotation(Count)
                {
                    for (int K = 0; K < Count; ++K)
                        Rotation[K] = std::polar(1.0, Pi * K / (2.0 * Count));
                    if (std::has_single_bit(Spectrum.size()))
                        return;

                    // Bluestein의 합성곱으로 직사각형 원본 PNG도 리사이즈 없이 적분한다.
                    const auto Size = Spectrum.size();
                    Chirp.resize(Size);
                    Kernel.resize(std::bit_ceil(2 * Size - 1));
                    Scratch.resize(Kernel.size());
                    for (std::size_t K = 0; K < Size; ++K)
                    {
                        Chirp[K] = std::polar(1.0, -Pi * static_cast<double>(K * K) / Size);
                        Kernel[K] = std::conj(Chirp[K]);
                        if (K != 0)
                            Kernel[Kernel.size() - K] = Kernel[K];
                    }
                    FourierPowerOfTwo(Kernel);
                }

                void Apply(std::span<double> Line, bool bInverse)
                {
                    const auto Count = Line.size();
                    if (!bInverse)
                    {
                        for (std::size_t Index = 0; Index < Count; ++Index)
                            Spectrum[Index] = Spectrum[Spectrum.size() - 1 - Index] = Line[Index];
                        Fourier();
                        for (std::size_t K = 0; K < Count; ++K)
                            Line[K] = 0.5 * std::real(Spectrum[K] * std::conj(Rotation[K]));
                        return;
                    }
                    Spectrum[0] = 2.0 * Line[0];
                    Spectrum[Count] = 0.0;
                    for (std::size_t K = 1; K < Count; ++K)
                    {
                        Spectrum[K] = 2.0 * Line[K] * std::conj(Rotation[K]);
                        Spectrum[Spectrum.size() - K] = std::conj(Spectrum[K]);
                    }
                    Fourier();
                    for (std::size_t Index = 0; Index < Count; ++Index)
                        Line[Index] = Spectrum[Index].real() / Spectrum.size();
                }

                private:
                void Fourier()
                {
                    if (Kernel.empty())
                    {
                        FourierPowerOfTwo(Spectrum);
                        return;
                    }
                    std::ranges::fill(Scratch, FComplex{});
                    for (std::size_t K = 0; K < Spectrum.size(); ++K)
                        Scratch[K] = Spectrum[K] * Chirp[K];
                    FourierPowerOfTwo(Scratch);
                    for (std::size_t K = 0; K < Scratch.size(); ++K)
                        Scratch[K] *= Kernel[K];
                    FourierPowerOfTwo(Scratch, true);
                    for (std::size_t K = 0; K < Spectrum.size(); ++K)
                        Spectrum[K] = Scratch[K] * Chirp[K];
                }

                std::vector<FComplex> Spectrum;
                std::vector<FComplex> Rotation;
                std::vector<FComplex> Chirp;
                std::vector<FComplex> Kernel;
                std::vector<FComplex> Scratch;
            };

            void IntegrateNormals(const Asset::FTextureData &Image, Liquid::FHeightfield &Shape)
            {
                const int              Columns = Shape.Columns, Rows = Shape.Rows;
                const double           Dx = Shape.PitchX(), Dy = Shape.PitchY();
                std::vector<glm::vec2> Slopes(Shape.Heights.size());
                for (int Y = 0; Y < Rows; ++Y)
                    for (int X = 0; X < Columns; ++X)
                    {
                        const glm::vec3 N = 2.F * texel(Image, (X + 0.5F) / Columns, (Y + 0.5F) / Rows) - 1.F;
                        if (!(N.z > 0.F))
                            throw std::runtime_error("Normal integration needs normals facing the positive chart Z axis.");
                        // 거의 수직인 노멀의 작은 양자화 오차가 높이를 지배하는 것을 막는다.
                        Slopes[Y * Columns + X] = -glm::vec2(N) / std::max(N.z, 0.05F);
                    }

                std::vector<double> Heights(Slopes.size());
                for (int Y = 0; Y < Rows; ++Y)
                    for (int X = 0; X < Columns; ++X)
                    {
                        const int Here = Y * Columns + X;
                        if (X + 1 < Columns)
                        {
                            const double Edge = 0.5 * (Slopes[Here].x + Slopes[Here + 1].x) / Dx;
                            Heights[Here] -= Edge;
                            Heights[Here + 1] += Edge;
                        }
                        if (Y + 1 < Rows)
                        {
                            const double Edge = 0.5 * (Slopes[Here].y + Slopes[Here + Columns].y) / Dy;
                            Heights[Here] -= Edge;
                            Heights[Here + Columns] += Edge;
                        }
                    }

                // 직사각형 자유 경계의 이웃 높이차 최소제곱을 코사인 기저에서 대각화한다.
                FCosineTransform    Across(Columns), Along(Rows);
                std::vector<double> Column(Rows);
                const auto          Transform = [&](bool bInverse)
                {
                    for (int Y = 0; Y < Rows; ++Y)
                        Across.Apply(std::span(Heights).subspan(Y * Columns, Columns), bInverse);
                    for (int X = 0; X < Columns; ++X)
                    {
                        for (int Y = 0; Y < Rows; ++Y)
                            Column[Y] = Heights[Y * Columns + X];
                        Along.Apply(Column, bInverse);
                        for (int Y = 0; Y < Rows; ++Y)
                            Heights[Y * Columns + X] = Column[Y];
                    }
                };
                const auto Eigenvalues = [](int Count, double Pitch)
                {
                    std::vector<double> Values(Count);
                    for (int K = 1; K < Count; ++K)
                    {
                        const double Frequency = 2.0 * std::sin(Pi * K / (2.0 * Count)) / Pitch;
                        Values[K] = Frequency * Frequency;
                    }
                    return Values;
                };
                const auto EigenX = Eigenvalues(Columns, Dx), EigenY = Eigenvalues(Rows, Dy);
                Transform(false);
                for (int Y = 0; Y < Rows; ++Y)
                    for (int X = 0; X < Columns; ++X)
                        if (X != 0 || Y != 0)
                            Heights[Y * Columns + X] /= EigenX[X] + EigenY[Y];
                Heights[0] = 0.0; // 주어진 기울기는 평균 높이를 결정하지 않는다.
                Transform(true);
                std::ranges::transform(Heights,
                                       Shape.Heights.begin(),
                                       [](double Height) { return static_cast<float>(Height); });
            }

            void ResampleNormalHeights(const Liquid::FHeightfield &Source, Liquid::FHeightfield &Target, float Scale)
            {
                const double StepX = static_cast<double>(Source.Columns) / Target.Columns;
                const double StepY = static_cast<double>(Source.Rows) / Target.Rows;
                for (int Y = 0; Y < Target.Rows; ++Y)
                    for (int X = 0; X < Target.Columns; ++X)
                    {
                        const double Left = X * StepX, Right = (X + 1) * StepX;
                        const double Bottom = Y * StepY, Top = (Y + 1) * StepY;
                        double       Sum = 0.0;
                        for (int J = static_cast<int>(Bottom); J < std::min(Source.Rows, static_cast<int>(std::ceil(Top)));
                             ++J)
                            for (int I = static_cast<int>(Left);
                                 I < std::min(Source.Columns, static_cast<int>(std::ceil(Right)));
                                 ++I)
                            {
                                const double Area = (std::min(Right, I + 1.0) - std::max(Left, static_cast<double>(I))) *
                                                    (std::min(Top, J + 1.0) - std::max(Bottom, static_cast<double>(J)));
                                Sum += Area * Source.Height(I, J);
                            }
                        Target.Heights[Y * Target.Columns + X] = static_cast<float>(Sum * Scale / (StepX * StepY));
                    }
            }

            void LoadNormalHeightfield(const std::filesystem::path &Path,
                                       Liquid::FHeightfield        &Target,
                                       float                        Scale,
                                       glm::ivec2                   IntegrationSamples)
            {
                struct FNormalHeightCache
                {
                    std::filesystem::path           Path;
                    std::filesystem::file_time_type Modified;
                    glm::ivec2                      Requested{};
                    Liquid::FHeightfield            Heights;
                };
                static std::mutex         Mutex;
                static FNormalHeightCache Cache;
                const auto                Modified = std::filesystem::last_write_time(Path);
                const std::scoped_lock    Lock(Mutex);
                if (Cache.Path != Path || Cache.Modified != Modified || Cache.Requested != IntegrationSamples ||
                    Cache.Heights.Width != Target.Width || Cache.Heights.Length != Target.Length)
                {
                    const auto Image = Asset::FTextureLoader::LoadRGBA8(Path);
                    const auto Size =
                        IntegrationSamples == glm::ivec2{} ? glm::ivec2(Image.Width, Image.Height) : IntegrationSamples;
                    if (Size.x < 2 || Size.y < 2 || Size.x > MaxNormalIntegrationAxis || Size.y > MaxNormalIntegrationAxis)
                        throw std::invalid_argument("Choose a normal integration grid between 2 and 2048 per axis.");
                    Liquid::FHeightfield Heights{Size.x,
                                                 Size.y,
                                                 Target.Width,
                                                 Target.Length,
                                                 std::vector<float>(static_cast<std::size_t>(Size.x) * Size.y)};
                    IntegrateNormals(Image, Heights);
                    Cache = {Path, Modified, IntegrationSamples, std::move(Heights)};
                }
                ResampleNormalHeights(Cache.Heights, Target, Scale);
            }

            void ProjectMesh(const std::filesystem::path &Path, Liquid::FHeightfield &Shape, float Scale)
            {
                const auto Mesh = Asset::FOBJLoader::Load(Path);
                glm::vec3  Lower(std::numeric_limits<float>::max()), Upper(std::numeric_limits<float>::lowest());
                for (const auto &Vertex : Mesh.Vertices)
                {
                    Lower = glm::min(Lower, Vertex.Position);
                    Upper = glm::max(Upper, Vertex.Position);
                }
                const glm::vec3 Extent = Upper - Lower;
                if (!std::isfinite(Extent.x) || !std::isfinite(Extent.y) || Extent.x <= 0.F || Extent.y <= 0.F)
                    throw std::runtime_error("OBJ must cover a rectangular single-valued XY chart.");
                std::fill(Shape.Heights.begin(), Shape.Heights.end(), std::numeric_limits<float>::quiet_NaN());
                const auto Point = [&](std::uint32_t Index)
                {
                    const auto P = Mesh.Vertices[Index].Position;
                    return glm::vec3((P.x - Lower.x) / Extent.x * Shape.Columns - 0.5F,
                                     (P.y - Lower.y) / Extent.y * Shape.Rows - 0.5F,
                                     (P.z - Lower.z) * Scale);
                };
                auto Cross = [](glm::vec3 A, glm::vec3 B) { return A.x * B.y - A.y * B.x; };
                for (std::size_t Index = 0; Index + 2 < Mesh.Indices.size(); Index += 3)
                {
                    const auto  A = Point(Mesh.Indices[Index]), B = Point(Mesh.Indices[Index + 1]),
                                C = Point(Mesh.Indices[Index + 2]);
                    const float Area = Cross(B - A, C - A);
                    if (std::abs(Area) < 1.e-9F)
                        continue;
                    for (int Y = std::max(0, static_cast<int>(std::ceil(std::min({A.y, B.y, C.y}))));
                         Y < Shape.Rows && Y <= static_cast<int>(std::floor(std::max({A.y, B.y, C.y})));
                         ++Y)
                        for (int X = std::max(0, static_cast<int>(std::ceil(std::min({A.x, B.x, C.x}))));
                             X < Shape.Columns && X <= static_cast<int>(std::floor(std::max({A.x, B.x, C.x})));
                             ++X)
                        {
                            const glm::vec3 P(static_cast<float>(X), static_cast<float>(Y), 0.F);
                            const float     U = Cross(P - A, C - A) / Area;
                            const float     V = Cross(B - A, P - A) / Area;
                            if (U < -1.e-5F || V < -1.e-5F || U + V > 1.00001F)
                                continue;
                            const float Z = A.z + U * (B.z - A.z) + V * (C.z - A.z);
                            float      &Sample = Shape.Heights[Y * Shape.Columns + X];
                            if (std::isfinite(Sample) && std::abs(Z - Sample) > 1.e-6F)
                                throw std::runtime_error("OBJ has overlapping layers in the selected XY chart.");
                            Sample = Z;
                        }
                }
                if (!std::all_of(Shape.Heights.begin(), Shape.Heights.end(), [](float Z) { return std::isfinite(Z); }))
                    throw std::runtime_error("OBJ does not cover the full XY sample rectangle.");
            }

            Liquid::FHeightfield Filter(Liquid::FHeightfield Shape, int Iterations)
            {
                Liquid::FHeightfield Next = Shape;
                for (int Iteration = 0; Iteration < Iterations; ++Iteration)
                    for (const float Gain : {0.50F, -0.53F})
                    {
                        for (int Y = 0; Y < Shape.Rows; ++Y)
                            for (int X = 0; X < Shape.Columns; ++X)
                            {
                                const float Current = Shape.Height(X, Y);
                                float       Shift = 0.F;
                                int         Count = 0;
                                for (const auto Offset : Neighbors)
                                {
                                    const int I = X + Offset[0], J = Y + Offset[1];
                                    if (I >= 0 && I < Shape.Columns && J >= 0 && J < Shape.Rows)
                                    {
                                        Shift += Shape.Height(I, J) - Current;
                                        ++Count;
                                    }
                                }
                                Next.Heights[Y * Shape.Columns + X] = Current + Gain * Shift / std::max(1, Count);
                            }
                        Shape.Heights.swap(Next.Heights);
                    }
                return Shape;
            }
        } // namespace

    #pragma endregion

    // ============================================================
    // 01. Heightfield Import
    // ============================================================
    #pragma region 01_Heightfield_Import
        // 입력 종류에 맞는 변환으로 하나의 직사각형 샘플 격자를 채운다.

        Liquid::FHeightfield LoadHeightfield(EHeightfieldSource           Source,
                                             const std::filesystem::path &Path,
                                             int                          Columns,
                                             int                          Rows,
                                             float                        Width,
                                             float                        Length,
                                             float                        Scale,
                                             glm::ivec2                   IntegrationSamples)
        {
            if (Columns < 1 || Rows < 1 || Columns > Liquid::MaxSamplesPerAxis || Rows > Liquid::MaxSamplesPerAxis ||
                !std::isfinite(Scale) || Scale <= 0.F || !(Width > 0.F) || !(Length > 0.F) || !std::isfinite(Width) ||
                !std::isfinite(Length) || Source == EHeightfieldSource::Procedural)
                throw std::invalid_argument("Invalid heightfield source or dimensions.");
            Liquid::FHeightfield Shape{Columns,
                                       Rows,
                                       Width,
                                       Length,
                                       std::vector<float>(static_cast<std::size_t>(Columns * Rows))};
            if (Source == EHeightfieldSource::HeightMap)
                SampleHeightMap(Path, Shape, Scale);
            else if (Source == EHeightfieldSource::Mesh)
                ProjectMesh(Path, Shape, Scale);
            else if (Source == EHeightfieldSource::NormalMap)
                LoadNormalHeightfield(Path, Shape, Scale, IntegrationSamples);
            else
            {
                const auto Image = Asset::FTextureLoader::LoadRGBA8(Path);
                for (int Y = 0; Y < Rows; ++Y)
                    for (int X = 0; X < Columns; ++X)
                    {
                        const float Value = texel(Image, (X + 0.5F) / Columns, (Y + 0.5F) / Rows).r;
                        Shape.Heights[Y * Columns + X] = (2.F * Value - 1.F) * Scale;
                    }
            }
            return Shape;
        }

    #pragma endregion

    // ============================================================
    // 02. Heightfield Bands
    // ============================================================
    #pragma region 02_Heightfield_Bands
        // 완화한 높이 필드의 차이로 입력을 재구성할 수 있는 대역을 만든다.

        FHeightfieldBands SplitHeightfield(const Liquid::FHeightfield &Shape, int BaseIterations, int MesoIterations)
        {
            if (MesoIterations < 0 || BaseIterations < MesoIterations || BaseIterations > 1024)
                throw std::invalid_argument("Geometry bandwidths must be ordered.");
            FHeightfieldBands Result{Filter(Shape, BaseIterations), Filter(Shape, MesoIterations), Shape};
            for (std::size_t Index = 0; Index < Shape.Heights.size(); ++Index)
            {
                Result.Micro.Heights[Index] -= Result.Meso.Heights[Index];
                Result.Meso.Heights[Index] -= Result.Base.Heights[Index];
            }
            return Result;
        }

    #pragma endregion

} // namespace MDSS::SurfaceState
