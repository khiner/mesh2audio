!> \\file testModule.f
module m_global_parameters

    implicit none

    integer :: vertices
    integer :: elements
    integer :: edges

    real(kind(0d0)), allocatable, dimension(:) :: dofs  !< degrees of freedom
    real(kind(0d0)), allocatable, dimension(:, :) :: M  !< Mass matrix
    real(kind(0d0)), allocatable, dimension(:, :) :: S  !< Stiffness matrix
    real(kind(0d0)), allocatable, dimension(:, :) :: ELC !< Element connectivity mnatrix
    real(kind(0d0)), allocatable, dimension(:, :) :: EDC !< Edge connectivity mnatrix

    integer :: i, j, k

    public

contains

    subroutine s_initialize_global_parameters()

    end subroutine

end module