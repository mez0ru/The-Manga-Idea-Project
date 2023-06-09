import { AxiosError } from 'axios';
import { useState, useEffect } from 'react'
import { useParams } from 'react-router-dom';
import useAxiosPrivate from './hooks/useAxiosPrivate';
import { ThemeProvider } from '@mui/material/styles';
import DualPage from './Components/DualPage';
import Toolbar from '@mui/material/Toolbar';
import Typography from '@mui/material/Typography';
import IconButton from '@mui/material/IconButton';
import ArrowBack from '@mui/icons-material/ArrowBack';
import AppBar from '@mui/material/AppBar';
import Slide from '@mui/material/Slide';
import Slider from '@mui/material/Slider';
import { mdTheme } from './Dashboard';
import { useSearchParams, useNavigate } from 'react-router-dom';

export interface Page {
    i: number;
    isWide?: boolean; // to save bandwidth, treat it false implicitly if not set.
}

export interface ChapterInfo {
    name?: string;
    pages: Page[];
}

export default function Viewer() {

    // chapter id
    let { seriesId, id } = useParams();
    const navigate = useNavigate();

    const [chapterInfo, setChapterInfo] = useState<ChapterInfo>();
    const [hideBars, setHideBars] = useState(true);

    const [searchParams] = useSearchParams();

    const [currentId, setCurrentId] = useState(parseInt(searchParams.get("page") ?? '0'));

    const axiosPrivate = useAxiosPrivate();

    useEffect(() => {
        document.body.style.backgroundColor = 'black';
        const getImageIds = async () => {
            try {
                const response = await axiosPrivate.get(`/api/v2/chapter/${id}`);
                setChapterInfo(response?.data);
                console.log(response?.data);
            } catch (err) {
                if (err instanceof AxiosError) {
                    if (err.response?.status === 401 || err.response?.status === 403)
                        navigate('/login', { state: { from: location }, replace: true });
                    // if (!err?.response) {
                    //     setErrMsg('No Server Response');
                    // } else if (err.response?.status === 400) {
                    //     setErrMsg('Missing Email or Password');
                    // } else if (err.response?.status === 401) {
                    //     setErrMsg('Unauthorized');
                    // } else {
                    //     setErrMsg('Login Failed');
                    // }
                    // errRef.current?.focus();
                }
            }
        }

        getImageIds();

        return () => {
            setChapterInfo(undefined);
            document.body.style.removeProperty('backgroundColor');
        }
    }, []);

    const handleChange = (event: Event, newValue: number | number[]) => {
        if (typeof newValue === 'number') {
            setCurrentId(newValue);
        }
    };

    if (chapterInfo) {
        return (
            <ThemeProvider theme={mdTheme}>
                <Slide appear={false} direction="down" in={hideBars}>
                    <AppBar position="fixed">
                        <Toolbar variant="dense">
                            <IconButton edge="start" color="inherit" aria-label="menu" sx={{ mr: 2 }} onClick={() => navigate(`/series/${seriesId}`)}>
                                <ArrowBack />
                            </IconButton>
                            <Typography variant="h6" color="inherit" component="div">
                                {chapterInfo.name}
                            </Typography>
                        </Toolbar>
                    </AppBar>
                </Slide>
                <DualPage array={chapterInfo.pages} chapterId={parseInt(id!!)} onClick={() => setHideBars(x => !x)} currentId={currentId} setCurrentId={setCurrentId} />
                <Slide appear={false} direction="up" in={hideBars}>
                    <AppBar position="fixed" sx={{ top: 'auto', bottom: 0 }}>
                        <Toolbar variant="dense">

                            <Typography variant="body1" gutterBottom mr={2} mt={1}>
                                0
                            </Typography>
                            <Slider value={currentId} max={chapterInfo.pages.length - 1} onChange={handleChange} size="small" aria-label="Default" valueLabelDisplay="auto" />
                            <Typography variant="body1" gutterBottom ml={2} mt={1}>
                                {chapterInfo.pages.length - 1}
                            </Typography>
                        </Toolbar>
                    </AppBar>
                </Slide>
            </ThemeProvider>
        )
    } else
        return <></>
}
