import { useEffect, useState, useRef } from 'react'
import { useNavigate, useParams } from 'react-router-dom';
import useAxiosPrivate from './hooks/useAxiosPrivate';
import Grid from '@mui/material/Grid';
import ChapterCard from './Components/ChapterCard';
import { useMyOutletContext } from './Dashboard';
import { AxiosError } from 'axios';

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

    const [chapters, setChapters] = useState<Chapter[]>([])
    const navigate = useNavigate();

    useEffect(() => {
        return () => {
            setName('')
        }
    }, [])


    useEffect(() => {
        let isMounted = true;
        const controller = new AbortController();

        const getChapters = async () => {
            try {
                const response = await axiosPrivate.get(`/api/v2/series/${id}`, {
                    signal: controller.signal
                });
                console.log(response.data);
                if (isMounted) {
                    setChapters(response.data.chapters as Chapter[]);
                    setName(response.data.name);
                    setChapters(x => x.map(y => Object.assign(y, { seriesId: parseInt(id!!) })));
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
        setInvalidate(false);

        return () => {
            isMounted = false;
            controller.abort();
        }
    }, [invalidate])

    return (<div>
        <Grid container spacing={2} px={{ xs: 3, sm: 2, md: 2, lg: 2 }} direction="row">
            {
                chapters.map((item, i) => (
                    <Grid item key={i}>
                        <ChapterCard chapter={item} />
                    </Grid>))
            }
        </Grid>
    </div>)
}
