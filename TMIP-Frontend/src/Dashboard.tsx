import * as React from 'react';
import { styled, createTheme, ThemeProvider } from '@mui/material/styles';
import CssBaseline from '@mui/material/CssBaseline';
import MuiDrawer from '@mui/material/Drawer';
import Box from '@mui/material/Box';
import MuiAppBar, { AppBarProps as MuiAppBarProps } from '@mui/material/AppBar';
import Toolbar from '@mui/material/Toolbar';
import List from '@mui/material/List';
import Typography from '@mui/material/Typography';
import Divider from '@mui/material/Divider';
import IconButton from '@mui/material/IconButton';
import Badge from '@mui/material/Badge';
import Container from '@mui/material/Container';
import Grid from '@mui/material/Grid';
import Link from '@mui/material/Link';
import MenuIcon from '@mui/icons-material/Menu';
import Add from '@mui/icons-material/Add';
import Edit from '@mui/icons-material/Edit';
import Delete from '@mui/icons-material/Delete';
import Refresh from '@mui/icons-material/Refresh';
import ChevronLeftIcon from '@mui/icons-material/ChevronLeft';
import NotificationsIcon from '@mui/icons-material/Notifications';
import Tooltip from '@mui/material/Tooltip';
import { MainListItems, secondaryListItems } from './listItems';
import { Outlet, useOutletContext, useLocation, useParams } from "react-router-dom";
import AddSeries from './Components/AddSeriesDialog';
import LinearProgress from '@mui/material/LinearProgress';
import useAxiosPrivate from './hooks/useAxiosPrivate';
import { AxiosError } from 'axios';
import DeleteSeriesDialog from './Components/DeleteSeriesDialog';
import UpdatedInfoDialog, { CodeItemInfo } from './Components/UpdatedInfoDialog';
import { Chapter } from './ChaptersPage';

function Copyright(props: any) {
  return (
    <Typography variant="body2" color="text.secondary" align="center" {...props}>
      {'Copyright © '}
      <Link color="inherit" href="https://manga-idea.com">
        The Manga Idea Project
      </Link>{' '}
      {new Date().getFullYear()}
      {'.'}
    </Typography>
  );
}

let drawerWidth: number = 240;

interface AppBarProps extends MuiAppBarProps {
  open?: boolean;
}

const AppBar = styled(MuiAppBar, {
  shouldForwardProp: (prop) => prop !== 'open',
})<AppBarProps>(({ theme, open }) => ({
  zIndex: theme.zIndex.drawer + 1,
  transition: theme.transitions.create(['width', 'margin'], {
    easing: theme.transitions.easing.sharp,
    duration: theme.transitions.duration.leavingScreen,
  }),
  ...(open && {
    marginLeft: drawerWidth,
    width: `calc(100% - ${drawerWidth}px)`,
    transition: theme.transitions.create(['width', 'margin'], {
      easing: theme.transitions.easing.sharp,
      duration: theme.transitions.duration.enteringScreen,
    }),
  }),
}));

const Drawer = styled(MuiDrawer, { shouldForwardProp: (prop) => prop !== 'open' })(
  ({ theme, open }) => ({
    '& .MuiDrawer-paper': {
      position: 'relative',
      whiteSpace: 'nowrap',
      width: drawerWidth,
      transition: theme.transitions.create('width', {
        easing: theme.transitions.easing.sharp,
        duration: theme.transitions.duration.enteringScreen,
      }),
      boxSizing: 'border-box',
      ...(!open && {
        overflowX: 'hidden',
        transition: theme.transitions.create('width', {
          easing: theme.transitions.easing.sharp,
          duration: theme.transitions.duration.leavingScreen,
        }),
        width: theme.spacing(7),
        [theme.breakpoints.up('sm')]: {
          width: theme.spacing(9),
        },
      }),
    },
  }),
);

export const mdTheme = createTheme({
  palette: {
    mode: 'dark',
  },
});

type MyContextType = { invalidate: boolean; setInvalidate: React.Dispatch<React.SetStateAction<boolean>>; isLoading: boolean; setIsLoading: React.Dispatch<React.SetStateAction<boolean>>; rescanChapters: boolean; setTaskId: React.Dispatch<React.SetStateAction<number>>; setRescanChapters: React.Dispatch<React.SetStateAction<boolean>>; setName: React.Dispatch<React.SetStateAction<string>> };

export function useMyOutletContext() {
  return useOutletContext<MyContextType>();
}

function DashboardContent() {

  const [open, setOpen] = React.useState(false);
  const [addSeriesActive, setAddSeriesActive] = React.useState(false);
  const [deleteSeriesDialog, setDeleteSeriesDialog] = React.useState(false);
  const [invalidate, setInvalidate] = React.useState(true);
  const [isLoading, setIsLoading] = React.useState(false);
  const [taskId, setTaskId] = React.useState<number | undefined>();
  const [taskName, setTaskName] = React.useState<string | undefined>();
  const [showInfo, setShowInfo] = React.useState(false);
  const [name, setName] = React.useState('');
  const [addedChapters, setAddedChapters] = React.useState<Chapter[]>([])
  const { pathname } = useLocation();
  const axiosPrivate = useAxiosPrivate();
  let { id } = useParams();

  drawerWidth = pathname === '/login' || pathname === '/register' ? 0 : 240;

  const toggleDrawer = () => {
    setOpen(!open);
  };

  React.useEffect(() => {
    let intervalFun: number;
    const checkIfTaskIsFinished = async () => {
      try {
        const response = await axiosPrivate.post('/api/v2/series', { id: id ? id : taskId });
        if (response.status === 200) { // means task is finished
          console.log('data: ');
          console.log(response.data);
          console.log(response.data.chapters);
          clearInterval(intervalFun);
          setAddedChapters(response.data.chapters);
          setTaskName(response.data.name);
          setShowInfo(true);
          setIsLoading(false);
          // setInvalidate(true);
        }
        setInvalidate(true);
      } catch (err: unknown) {
        if (err instanceof AxiosError) {
          console.log(err.response);
          clearInterval(intervalFun);
        }
      }
    }

    if (isLoading && (taskId || id)) {
      checkIfTaskIsFinished();
      intervalFun = setInterval(checkIfTaskIsFinished, 2000);
    }

    return () => {
      clearInterval(intervalFun);
    }
  }, [isLoading])


  const reScanChapters = async (id: number | undefined) => {
    if (id !== undefined) {
      setIsLoading(true);

      try {
        const response = await axiosPrivate.put(`/api/v2/series`, {
          id
        });
        setInvalidate(true);
      } catch (err: unknown) {
        if (err instanceof AxiosError) {
          console.log(err);
        }
      }

      setIsLoading(false);
    }
  }

  return (
    <ThemeProvider theme={mdTheme}>
      <Box sx={{ display: 'flex' }}>
        <CssBaseline />
        <AppBar position="absolute" open={open}>
          <Toolbar
            sx={{
              pr: '24px', // keep right padding when drawer closed
            }}
          >
            <IconButton
              edge="start"
              color="inherit"
              aria-label="open drawer"
              onClick={toggleDrawer}
              sx={{
                marginRight: '36px',
                ...(open && { display: 'none' }),
              }}
            >
              <MenuIcon />
            </IconButton>
            <Typography
              component="h1"
              variant="h6"
              color="inherit"
              noWrap
              sx={{ flexGrow: 1 }}
            >
              {name ? name : 'Manga Idea'}
            </Typography>
            {pathname === '/home' ? <Tooltip title="Add a new series">
              <IconButton color="inherit" onClick={() => setAddSeriesActive(true)} >
                <Add />
              </IconButton>
            </Tooltip> : null}
            {pathname.match(/^\/series\/\d{0,}$/g) ? <><Tooltip title="Rescan series folder for new chapters">
              <IconButton color="inherit" onClick={() => reScanChapters(id ? parseInt(id) : undefined)} disabled={isLoading}>
                <Refresh />
              </IconButton>
            </Tooltip>
              <Tooltip title="Edit Series">
                <IconButton color="inherit" onClick={() => setDeleteSeriesDialog(true)} disabled={isLoading}>
                  <Edit />
                </IconButton>
              </Tooltip>
              <Tooltip title="Delete Series">
                <IconButton color="inherit" onClick={() => setDeleteSeriesDialog(true)} disabled={isLoading}>
                  <Delete />
                </IconButton>
              </Tooltip></> : null}
            {/* <IconButton color="inherit">
              <Badge badgeContent={4} color="secondary">
                <NotificationsIcon />
              </Badge>
            </IconButton> */}
          </Toolbar>
        </AppBar>
        {pathname === '/login' || pathname === '/register' ? null :
          <Drawer variant="permanent" open={open} ModalProps={{
            keepMounted: true, // Better open performance on mobile.
          }}>
            <Toolbar
              sx={{
                display: 'flex',
                alignItems: 'center',
                justifyContent: 'flex-end',
                px: [1],
              }}
            >
              <IconButton onClick={toggleDrawer}>
                <ChevronLeftIcon />
              </IconButton>
            </Toolbar>
            <Divider />
            <List component="nav">
              <MainListItems />
              <Divider sx={{ my: 1 }} />
              {secondaryListItems}
            </List>
          </Drawer>}
        <Box
          component="main"
          sx={{
            backgroundColor: (theme) =>
              theme.palette.mode === 'light'
                ? theme.palette.grey[100]
                : theme.palette.grey[900],
            flexGrow: 1,
            height: '100vh',
            overflow: 'auto',
          }}
        >
          <Toolbar />
          {isLoading ? <LinearProgress variant='query' /> : null}
          <Container maxWidth="xl" sx={{ mt: 4, mb: 4 }}>
            <Grid container spacing={3} style={{ marginTop: 50 }}>

              <Outlet context={{ invalidate, setInvalidate, setName, setIsLoading, setTaskId }} />
              <AddSeries open={addSeriesActive} setOpen={setAddSeriesActive} setInvalidate={setInvalidate} />
              <DeleteSeriesDialog open={deleteSeriesDialog} setOpen={setDeleteSeriesDialog} name={name} id={id ? parseInt(id) : undefined} />
              <UpdatedInfoDialog open={showInfo} setOpen={setShowInfo} title="Task Completed" message={
                <CodeItemInfo elevation={3}>
                  Series "{taskName}" has been added with chapters:<br />
                  {addedChapters.map((x: Chapter, index: number) =>
                    <>{`Chapter ${index + 1}: ${x.file_name ?? 'Not added'}`}<br />{`Pages: ${x.pages_count ?? 0}`}{x.error ? <><br /><p style={{ color: 'red' }}>{`Error: ${x.error}`}</p></> : null}<br /><br /></>)}
                </CodeItemInfo>
              } />
            </Grid>
            <Copyright sx={{ pt: 4 }} />
          </Container>
        </Box>
      </Box>
    </ThemeProvider>
  );
}

export default function Dashboard() {
  return <DashboardContent />;
}
