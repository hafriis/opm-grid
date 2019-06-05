#include <config.h>

// Warning suppression for Dune includes.
#include <opm/grid/utility/platform_dependent/disable_warnings.h>

#include <dune/common/unused.hh>
#include <opm/grid/CpGrid.hpp>
#include <opm/grid/polyhedralgrid.hh>
#include <opm/grid/cpgrid/GridHelpers.hpp>
#include <dune/grid/io/file/vtk/vtkwriter.hh>

#include <opm/grid/cpgrid/dgfparser.hh>
#include <opm/grid/polyhedralgrid/dgfparser.hh>

#include <opm/grid/verteq/topsurf.hpp>

#include <opm/grid/common/VerteqColumnUtility.hpp>

#define DISABLE_DEPRECATED_METHOD_CHECK 1
#if DUNE_VERSION_NEWER(DUNE_GRID,2,5)
#include <dune/grid/test/gridcheck.hh>
#endif

// Re-enable warnings.
#include <opm/grid/utility/platform_dependent/reenable_warnings.h>

#include <opm/grid/utility/OpmParserIncludes.hpp>

#include <iostream>

const char *deckString =
    "RUNSPEC\n"
    "METRIC\n"
    "DIMENS\n"
    "2 2 2 /\n"
    "GRID\n"
    "DXV\n"
    "2*1 /\n"
    "DYV\n"
    "2*1 /\n"
    "DZ\n"
    "8*1 /\n"
    "TOPS\n"
    "4*100.0 /\n";

template <class GridView>
void testGridIteration( const GridView& gridView )
{
    typedef typename GridView::template Codim<0>::Iterator ElemIterator;
    typedef typename GridView::IntersectionIterator IsIt;
    typedef typename GridView::template Codim<0>::Geometry Geometry;
    typedef typename GridView::Grid::LocalIdSet LocalIdSet;

    const LocalIdSet& localIdSet = gridView.grid().localIdSet();

    int numElem = 0;
    ElemIterator elemIt = gridView.template begin<0>();
    ElemIterator elemEndIt = gridView.template end<0>();
    for (; elemIt != elemEndIt; ++elemIt) {
        const Geometry& elemGeom = elemIt->geometry();
        if (std::abs(elemGeom.volume() - 1.0) > 1e-8)
            std::cout << "element's " << numElem << " volume is wrong:"<<elemGeom.volume()<<"\n";

        typename Geometry::LocalCoordinate local( 0.5 );
        typename Geometry::GlobalCoordinate global = elemGeom.global( local );
        typename Geometry::GlobalCoordinate center = elemGeom.center();
        if( (center - global).two_norm() > 1e-6 )
        {
          std::cout << "center = " << center << " global( localCenter ) = " << global << std::endl;
        }


        int numIs = 0;
        IsIt isIt = gridView.ibegin(*elemIt);
        IsIt isEndIt = gridView.iend(*elemIt);
        for (; isIt != isEndIt; ++isIt, ++ numIs)
        {
            const auto& intersection = *isIt;
            const auto& isGeom = intersection.geometry();
            std::cout << "Checking intersection id = " << localIdSet.id( intersection ) << std::endl;
            if (std::abs(isGeom.volume() - 1.0) > 1e-8)
                std::cout << "volume of intersection " << numIs << " of element " << numElem << " volume is wrong: " << isGeom.volume() << "\n";

            if (intersection.neighbor())
            {
              if( numIs != intersection.indexInInside() )
                  std::cout << "num iit = " << numIs << " indexInInside " << intersection.indexInInside() << std::endl;
#if DUNE_VERSION_NEWER(DUNE_GRID,2,4)
              if (std::abs(intersection.outside().geometry().volume() - 1.0) > 1e-8)
                  std::cout << "outside element volume of intersection " << numIs << " of element " << numElem
                            << " volume is wrong: " << intersection.outside().geometry().volume() << std::endl;

              if (std::abs(intersection.inside().geometry().volume() - 1.0) > 1e-8)
                  std::cout << "inside element volume of intersection " << numIs << " of element " << numElem
                            << " volume is wrong: " << intersection.inside().geometry().volume() << std::endl;
#endif
            }
        }

        if (numIs != 2 * GridView::dimension )
            std::cout << "number of intersections is wrong for element " << numElem << "\n";

        ++ numElem;
    }

    if (numElem != 2*2*2)
        std::cout << "number of elements is wrong: " << numElem << "\n";
}

template <class Grid>
void testVerteq(const Grid& grid)
{
    typedef typename Grid::LeafGridView GridView;
    GridView gv = grid.leafGridView();

    Dune::VerteqColumnUtility< Grid > verteqUtil ( grid );

    std::cout << "Start checking vertical equilibirum utility." << std::endl;

    const auto end = gv.template end< 0 > ();
    for( auto it = gv.template begin< 0 > (); it != end; ++it )
    {
      const auto& entity = *it ;
      std::cout << "Start column for entity " << entity.impl().index() << std::endl;
      const auto endCol = verteqUtil.end( entity );
      for( auto col = verteqUtil.begin( entity ); col != endCol; ++col )
      {
        const auto& collCell = *col;
        std::cout << "Column cell [ " << collCell.index()
                  << " ]: h = " << collCell.h()
                  << " dz = "   << collCell.dz() << std::endl;
      }
    }
    std::cout << "Finished checking vertical equilibirum utility." << std::endl;
}


template <class Grid>
void testGrid(Grid& grid, const std::string& name)
{
    testVerteq( grid );

    typedef typename Grid::LeafGridView GridView;
#if DUNE_VERSION_NEWER(DUNE_GRID,2,5)
    try {
      gridcheck( grid );
    }
    catch ( const Dune::Exception& e)
    {
      std::cerr << "Warning: " << e.what() << std::endl;
    }
#endif
    std::cout << name << std::endl;

    testGridIteration( grid.leafGridView() );

    std::cout << "create vertex mapper\n";
    Dune::MultipleCodimMultipleGeomTypeMapper<GridView,
                                              Dune::MCMGVertexLayout> mapper(grid.leafGridView());

    std::cout << "VertexMapper.size(): " << mapper.size() << "\n";
    if (mapper.size() != 27) {
        std::cout << "Wrong size of vertex mapper. Expected 27!\n";
        //std::abort();
    }

    // VTKWriter does not work with geometry type none at the moment
    if( true || grid.geomTypes( 0 )[ 0 ].isCube() )
    {
      std::cout << "create vtkWriter\n";
      typedef Dune::VTKWriter<GridView> VtkWriter;
      VtkWriter vtkWriter(grid.leafGridView());

      std::cout << "create cellData\n";
      int numElems = grid.size(0);
      std::vector<double> tmpData(numElems, 0.0);

      std::cout << "add cellData\n";
      vtkWriter.addCellData(tmpData, name);

      std::cout << "write data\n";
      vtkWriter.write(name, Dune::VTK::ascii);
    }

}

int main(int argc, char** argv )
{
    // initialize MPI
    Dune::MPIHelper::instance( argc, argv );

    std::stringstream dgfFile;
    // create unit cube with 8 cells in each direction
    dgfFile << "DGF" << std::endl;
    dgfFile << "Interval" << std::endl;
    dgfFile << "0 0 0" << std::endl;
    dgfFile << "2 2 2" << std::endl;
    dgfFile << "2 2 2" << std::endl;
    dgfFile << "#" << std::endl;
    //dgfFile << "Simplex" << std::endl;
    //dgfFile << "#" << std::endl;

#if HAVE_ECL_INPUT
    Opm::Parser parser;
    const auto deck = parser.parseString(deckString);
    std::vector<double> porv;
#endif

    // test PolyhedralGrid
    {
      typedef Dune::PolyhedralGrid< 3, 3 > Grid;
#if HAVE_ECL_INPUT
      Grid grid(deck, porv);
      testGrid( grid, "polyhedralgrid" );
      Opm::TopSurf* ts;
      ts = Opm::TopSurf::create (grid);
      std::cout << ts->dimensions << std::endl;
      std::cout << ts->number_of_cells <<" " << ts->number_of_faces << " " << ts->number_of_nodes << " " << std::endl;
      //for (int i = 0; i < ts->number_of_nodes*ts->dimensions; ++i)
      //  std::cout << ts->node_coordinates[i] << std::endl;
      typedef Dune::PolyhedralGrid< 2, 2 > Grid2D;

      std::cout << "tsDune for " << std::endl;
      Grid2D tsDune (*ts);
      std::cout << "tsDune after " << std::endl;
      testGrid ( tsDune, "ts");
      return 0;

#endif
      Dune::GridPtr< Grid > gridPtr( dgfFile );
      testGrid( *gridPtr, "polyhedralgrid-dgf" );
    }

    /*
    // test CpGrid
    {
      typedef Dune::CpGrid Grid;
#if HAVE_ECL_INPUT
      Grid grid;
      const int* actnum = deck.hasKeyword("ACTNUM") ? deck.getKeyword("ACTNUM").getIntData().data() : nullptr;
      Opm::EclipseGrid ecl_grid(deck , actnum);

      grid.processEclipseFormat(ecl_grid, false, false, false, porv);
      testGrid( grid, "cpgrid" );

      Opm::UgGridHelpers::createEclipseGrid( grid , ecl_grid );
      testGrid( grid, "cpgrid2" );

#endif
      //Dune::GridPtr< Grid > gridPtr( dgfFile );
      //testGrid( *gridPtr, "cpgrid-dgf" );
    }
    */
    return 0;
}
