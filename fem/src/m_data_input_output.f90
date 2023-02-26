module m_data_input_output


    !< Dependencies 
    use m_global_parameters
    use stringmod
    use precmod

    implicit none
    
    integer :: Nv = 1  !< dof iterator
    integer :: Nel = 1 !< element iterator
    integer :: Ned     !< edge iterator
    integer:: io       !< iostat variable
    character (len=100) :: filecontents, msg, lineType !< strings for stuff

    private;  public :: s_read_mesh_file

contains

    !< This subroutine reads the mesh file and populates lots of matrices
    subroutine s_read_mesh_file()

        character filename*50
        call getarg(1, filename) !< gets filename from CLA

        open (unit = 1, file = filename, iostat=io)

        do
            read(1, "(a)", iostat=io, iomsg=msg) filecontents
            if (io/=0) exit

            call split(filecontents, " ", lineType)
            
            !< get vertex count for allocating matrices             
            if (lineType == "Vertices:") then
                call split(filecontents, " ", lineType)
                read(lineType, *) vertices
                allocate (dofs(1:vertices*3))
            end if


            !< get element count for allocating matrices
            if (linetype == "Elements:") then
                call split(filecontents, " ", lineType)
                read(lineType, *) elements
                allocate (ELC(1:elements, 8))
            end if

            !< get edge count for allocating matrices
            if (linetype == "Edges:") then
                call split(filecontents, " ", lineType)
                read(lineType, *) edges
                allocate (EDC(1:edges, 2))
            end if

            !< if line corresponds to a vertex location
            if (lineType == "v") then
                do j = 1, 3
                    call split(filecontents, " ", lineType)
                    read(lineType, *) dofs(Nv)
                    Nv = Nv + 1
                end do
            end if

            !< if lline corresponds to an element specification
            if (lineType == "E") then
                do j = 1, 8
                    call split(filecontents, " ", lineType)
                    read(lineType, *) ELC(Nel, j)
                end do
                Nel = Nel + 1
            end if

            !< if line corresponds to an edge specification
            if (linetype == "e") then
                do j = 1, 2
                    call split(filecontents, " ", lineType)
                    read(lineType, *) EDC(Nel, j)
                end do
                Ned = Ned + 1
            end if

        end do

    end subroutine s_read_mesh_file

end module m_data_input_output