import { useEffect, useState, useRef } from 'react'
import { useNavigate, useParams } from 'react-router-dom';
import useAxiosPrivate from './hooks/useAxiosPrivate';
import Grid from '@mui/material/Grid';
import ChapterCard from './Components/ChapterCard';
import { useMyOutletContext } from './Dashboard';
import { AxiosError } from 'axios';
import { useChaptersStore } from './store/ChaptersStore';

export interface Chapter {
    id: number;
    seriesId?: number;
    name: string;
    pages_count: number;
    error?: string;
    file_name?: string;
}

export default function ChaptersPage() {
    let { id } = useParams();
    const { invalidate, setInvalidate, setName } = useMyOutletContext();

    const axiosPrivate = useAxiosPrivate();

    // const [chapters, setChapters] = useState<Chapter[]>([])
    const chapters = useChaptersStore((state) => state.chapters)
    const setChapters = useChaptersStore((state) => state.setChapters)

    const scrollPosition = useChaptersStore((state) => state.scrollPosition)

    const navigate = useNavigate();

    useEffect(() => {
        return () => {
            setName('')
        }
    }, [])


    useEffect(() => {
        if (chapters.length === 0) {
            let isMounted = true;
            const controller = new AbortController();

            const getChapters = async () => {
                try {
                    const response = await axiosPrivate.get(`/api/v2/series/${id}`, {
                        signal: controller.signal
                    });
                    if (isMounted) {
                        // setChapters( as Chapter[]);
                        setName(response.data.name);
                        setChapters(response.data.chapters.map((y: Chapter) => Object.assign(y, { seriesId: parseInt(id!!) })));
                    }
                } catch (err) {
                    if (err instanceof AxiosError) {
                        console.error(err);
                        if (err.response?.status === 401 || err.response?.status === 403)
                            navigate('/login', { state: { from: location }, replace: true });
                    }
                }
            }


            getChapters();

            return () => {
                isMounted = false;
                controller.abort();
            }
        }
        setInvalidate(false);
    }, [invalidate])

    return (<div>
        <Grid container spacing={2} px={{ xs: 3, sm: 2, md: 2, lg: 2 }} direction="row">
            {
                chapters.filter(x => x.pages_count > 0).map((item, i) => (
                    <Grid item key={i}>
                        <ChapterCard chapter={item} index={i} />
                    </Grid>))
            }
        </Grid>
    </div>)
}
